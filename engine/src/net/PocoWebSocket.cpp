#include "net/PocoWebSocket.hpp"

#include <openssl/ssl.h>

#include <Poco/Buffer.h>
#include <Poco/Error.h>
#include <Poco/Exception.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/RejectCertificateHandler.h>
#include <Poco/Net/SSLException.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/URI.h>

#include <algorithm>
#include <stdexcept>
#include <thread>

#include "varn/tls/CaBundle.h"

namespace haylen::net {

std::unique_ptr<WebSocketTransport> WebSocketTransport::open(const std::string& url, const std::vector<std::string>& protocols, std::size_t maxMessageSize, Sink sink) {
    return std::make_unique<PocoWebSocket>(url, protocols, maxMessageSize, std::move(sink));
}

// OpenSSL starts before the workers finish constructing, so its cleanup at exit runs only after they finished.
PocoWebSocket::Workers::Workers() {
    OPENSSL_init_ssl(0, nullptr);
    Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> handler(new Poco::Net::RejectCertificateHandler(false));
    Poco::Net::SSLManager::instance().initializeClient(nullptr, handler, nullptr);
}

PocoWebSocket::Workers::~Workers() {
    std::vector<std::weak_ptr<Connection>> stopped;
    {
        const std::scoped_lock lock(mutex);
        exiting = true;
        stopped.swap(connections);
    }
    for (const std::weak_ptr<Connection>& weak : stopped) {
        if (const std::shared_ptr<Connection> alive = weak.lock()) {
            {
                const std::scoped_lock lock(alive->mutex);
                alive->stopping = true;
            }
            alive->poller.wakeUp();
        }
    }

    std::unique_lock lock(mutex);
    idle.wait(lock, [this] { return active == 0; });
}

void PocoWebSocket::Workers::add(const std::shared_ptr<Connection>& added) {
    const std::scoped_lock lock(mutex);
    std::erase_if(connections, [](const std::weak_ptr<Connection>& weak) { return weak.expired(); });
    connections.push_back(added);
}

bool PocoWebSocket::Workers::enter() {
    const std::scoped_lock lock(mutex);
    if (exiting) {
        return false;
    }
    ++active;
    return true;
}

// Notifying under the lock keeps the workers alive until the notification is done, since the destructor waits for the same lock.
void PocoWebSocket::Workers::leave() {
    const std::scoped_lock lock(mutex);
    if (--active == 0) {
        idle.notify_all();
    }
}

Poco::Net::Context::Ptr PocoWebSocket::Workers::getClientContext() {
    const std::scoped_lock lock(mutex);
    if (clientContext) {
        return clientContext;
    }
#if defined(_WIN32)
    clientContext = new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "", Poco::Net::Context::VERIFY_STRICT, Poco::Net::Context::OPT_DEFAULTS, Poco::Net::Context::CERT_STORE_ROOT);
#else
    const std::string& bundle = varn::tls::CaBundle::resolve();
    if (bundle.empty()) {
        throw std::runtime_error("No trust store was found to check the certificate of a \"wss://\" server: " + varn::tls::CaBundle::describeSearch() + ".");
    }
    clientContext = new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "", "", bundle, Poco::Net::Context::VERIFY_STRICT, 9, true, "DEFAULT@SECLEVEL=2");
#endif
    return clientContext;
}

PocoWebSocket::Workers& PocoWebSocket::getWorkers() {
    static Workers workers;
    return workers;
}

// The address is parsed here, so the thread never needs the static state of Poco that parsing uses.
PocoWebSocket::PocoWebSocket(const std::string& url, std::vector<std::string> protocols, std::size_t messageLimit, Sink target) : connection(std::make_shared<Connection>()) {
    connection->protocols = std::move(protocols);
    connection->maxMessageSize = messageLimit;
    connection->sink = std::move(target);
    try {
        const Poco::URI uri(url);
        connection->host = uri.getHost();
        connection->port = uri.getPort();
        connection->path = uri.getPathAndQuery().empty() ? "/" : uri.getPathAndQuery();
        connection->secure = uri.getScheme() == "wss";
    } catch (const Poco::Exception& error) {
        fail(*connection, error.displayText());
        return;
    }

    Workers& workers = getWorkers();
    workers.add(connection);
    std::thread([shared = connection, &workers] { run(shared, workers); }).detach();
}

PocoWebSocket::~PocoWebSocket() {
    {
        const std::scoped_lock lock(connection->mutex);
        connection->stopping = true;
    }
    connection->poller.wakeUp();
}

void PocoWebSocket::send(std::string data, bool binary) {
    {
        const std::scoped_lock lock(connection->mutex);
        connection->outgoing.push_back({std::move(data), binary ? Poco::Net::WebSocket::FRAME_BINARY : Poco::Net::WebSocket::FRAME_TEXT});
    }
    wake(*connection);
}

void PocoWebSocket::ping(std::string payload) {
    {
        const std::scoped_lock lock(connection->mutex);
        connection->outgoing.push_back({std::move(payload), kPingFrame});
    }
    wake(*connection);
}

void PocoWebSocket::close(int code, std::string reason) {
    {
        const std::scoped_lock lock(connection->mutex);
        connection->closeRequest = std::make_pair(code, std::move(reason));
    }
    wake(*connection);
}

// A wake-up still pending covers the next one, so the pipe behind the poll set never fills up however much the app sends.
void PocoWebSocket::wake(Connection& target) {
    {
        const std::scoped_lock lock(target.mutex);
        if (std::exchange(target.wakePending, true)) {
            return;
        }
    }
    target.poller.wakeUp();
}

void PocoWebSocket::report(Connection& target, Event event) {
    const std::scoped_lock lock(target.mutex);
    if (!target.stopping) {
        target.sink(std::move(event));
    }
}

void PocoWebSocket::fail(Connection& target, const std::string& message) {
    report(target, {.kind = Event::Kind::Failed, .text = message});
    report(target, {.kind = Event::Kind::Closed, .code = kAbnormalClosure});
}

void PocoWebSocket::run(const std::shared_ptr<Connection>& target, Workers& workers) {
    const std::optional<Poco::Net::SocketAddress> address = resolve(*target);
    if (!address || !enter(*target, workers)) {
        return;
    }
    connectAndServe(*target, workers, *address);
    workers.leave();
}

// Resolving has no timeout, so it runs before the thread counts as a worker, and a connection abandoned meanwhile ends right after it.
std::optional<Poco::Net::SocketAddress> PocoWebSocket::resolve(Connection& target) {
    try {
        return Poco::Net::SocketAddress(target.host, target.port);
    } catch (const Poco::Exception& error) {
        fail(target, error.displayText());
    } catch (const std::exception& error) {
        fail(target, error.what());
    }
    return std::nullopt;
}

bool PocoWebSocket::enter(Connection& target, Workers& workers) {
    const std::scoped_lock lock(target.mutex);
    return !target.stopping && workers.enter();
}

// Every Poco object of the connection is gone when this returns, so the thread uses neither library once it left the workers.
void PocoWebSocket::connectAndServe(Connection& target, Workers& workers, const Poco::Net::SocketAddress& address) {
    try {
        std::string protocol;
        const std::unique_ptr<Poco::Net::WebSocket> socket = open(target, workers, address, protocol);
        if (socket) {
            // Control frames carry up to 125 bytes under any message limit, and the serve loop checks the size of whole messages.
            socket->setMaxPayloadSize(static_cast<int>(std::max(target.maxMessageSize, kMaxControlPayload)));
            report(target, {.kind = Event::Kind::Opened, .text = std::move(protocol)});
            serve(target, *socket);
        }
    } catch (const Poco::Exception& error) {
        fail(target, error.displayText());
    } catch (const std::exception& error) {
        fail(target, error.what());
    }
    target.poller.clear();
}

// The poll set watches the one socket of the connection, which keeps its descriptor while Poco wraps it in TLS and a WebSocket, so any result means the socket is ready.
bool PocoWebSocket::waitFor(Connection& target, const Poco::Net::Socket& socket, int mode, std::chrono::steady_clock::time_point deadline) {
    if (target.poller.has(socket)) {
        target.poller.update(socket, mode);
    } else {
        target.poller.add(socket, mode);
    }
    const auto left = std::max(std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()), std::chrono::microseconds(0));
    return !target.poller.poll(Poco::Timespan(left.count())).empty();
}

bool PocoWebSocket::connectSocket(Connection& target, Poco::Net::StreamSocket& socket, const Poco::Net::SocketAddress& address, std::chrono::steady_clock::time_point deadline) {
    socket.connectNB(address);
    while (!target.stopping) {
        if (std::chrono::steady_clock::now() >= deadline) {
            throw Poco::TimeoutException("The connection to \"" + target.host + "\" timed out.");
        }
        if (!waitFor(target, socket, Poco::Net::PollSet::POLL_WRITE, deadline)) {
            continue;
        }
        const int error = socket.impl()->socketError();
        if (error != 0) {
            throw Poco::Net::NetException(Poco::Error::getMessage(error), address.toString(), error);
        }
        return true;
    }
    return false;
}

// The handshake runs without blocking, so abandoning the connection interrupts it. A finished handshake returns 1 with OpenSSL and 0 on Windows, while OpenSSL returns 0 for a connection the server closed, so a result that is not negative counts as finished once the server presented its certificate. Verifying the host name of the certificate is the step a handshake that does not block leaves to the caller.
bool PocoWebSocket::completeHandshake(Connection& target, Poco::Net::SecureStreamSocket& socket, std::chrono::steady_clock::time_point deadline) {
    while (!target.stopping) {
        const int result = socket.completeHandshake();
        if (result >= 0) {
            if (!socket.havePeerCertificate()) {
                throw Poco::Net::SSLConnectionUnexpectedlyClosedException();
            }
            socket.verifyPeerCertificate();
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            throw Poco::TimeoutException("The TLS handshake with \"" + target.host + "\" timed out.");
        }
        (void)waitFor(target, socket, result == Poco::Net::SecureStreamSocket::ERR_SSL_WANT_READ ? Poco::Net::PollSet::POLL_READ : Poco::Net::PollSet::POLL_WRITE, deadline);
    }
    return false;
}

// A secure socket connects without its handshake on every TLS library of Poco, while attaching TLS to a connected socket runs the whole handshake at once on Windows.
Poco::Net::StreamSocket PocoWebSocket::createSocket(const Connection& target, Workers& workers, const Poco::Net::SocketAddress& address) {
    if (!target.secure) {
        return Poco::Net::StreamSocket(address.family());
    }
    Poco::Net::SecureStreamSocket socket(workers.getClientContext());
    socket.setPeerHostName(target.host);
    return socket;
}

std::unique_ptr<Poco::Net::WebSocket> PocoWebSocket::open(Connection& target, Workers& workers, const Poco::Net::SocketAddress& address, std::string& protocol) {
    const auto deadline = std::chrono::steady_clock::now() + kConnectTimeout;
    Poco::Net::StreamSocket socket = createSocket(target, workers, address);
    if (!connectSocket(target, socket, address, deadline)) {
        return nullptr;
    }
    if (target.secure) {
        Poco::Net::SecureStreamSocket secure(socket);
        if (!completeHandshake(target, secure, deadline)) {
            return nullptr;
        }
    }

    // The upgrade request blocks, bounded by the timeouts of the socket, and the session sends it over the connected socket.
    const Poco::Timespan timeout(std::chrono::duration_cast<std::chrono::seconds>(kConnectTimeout).count(), 0);
    socket.setBlocking(true);
    socket.setSendTimeout(timeout);
    socket.setReceiveTimeout(timeout);
    Poco::Net::HTTPClientSession session(socket);
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, target.path, Poco::Net::HTTPMessage::HTTP_1_1);
    request.setHost(target.host, target.port);
    if (!target.protocols.empty()) {
        std::string list;
        for (const std::string& name : target.protocols) {
            list += (list.empty() ? "" : ", ") + name;
        }
        request.set("Sec-WebSocket-Protocol", list);
    }
    Poco::Net::HTTPResponse response;
    auto upgraded = std::make_unique<Poco::Net::WebSocket>(session, request, response);

    // Blocking reads would wait for a full header's worth of bytes even when a small frame already arrived.
    upgraded->setBlocking(false);
    protocol = response.get("Sec-WebSocket-Protocol", "");
    return upgraded;
}

void PocoWebSocket::write(Connection& target, Poco::Net::WebSocket& socket, std::string_view data, int flags) {
    while (socket.sendFrame(data.data(), static_cast<int>(data.size()), flags) < 0 && !target.stopping) {
        (void)waitFor(target, socket, Poco::Net::PollSet::POLL_WRITE, std::chrono::steady_clock::now() + kIdleWait);
    }
}

void PocoWebSocket::writeClose(Connection& target, Poco::Net::WebSocket& socket, int code, std::string_view reason) {
    std::string payload{static_cast<char>((code >> 8) & 0xFF), static_cast<char>(code & 0xFF)};
    payload += reason;
    write(target, socket, payload, kCloseFrame);
}

void PocoWebSocket::refuseMessage(Connection& target, Poco::Net::WebSocket& socket, bool closing) {
    if (!closing) {
        writeClose(target, socket, Poco::Net::WebSocket::WS_PAYLOAD_TOO_BIG, "");
    }
    report(target, {.kind = Event::Kind::Failed, .text = "The server sent a WebSocket message larger than the maximum of " + std::to_string(target.maxMessageSize) + " bytes."});
    report(target, {.kind = Event::Kind::Closed, .code = Poco::Net::WebSocket::WS_PAYLOAD_TOO_BIG});
}

void PocoWebSocket::serve(Connection& target, Poco::Net::WebSocket& socket) {
    Poco::Buffer<char> frame(0);
    std::string message;
    bool messageBinary = false;
    bool closing = false;
    std::chrono::steady_clock::time_point closeDeadline;

    while (!target.stopping) {
        std::vector<Outgoing> pending;
        std::optional<std::pair<int, std::string>> requestedClose;
        {
            const std::scoped_lock lock(target.mutex);
            pending.swap(target.outgoing);
            requestedClose = std::exchange(target.closeRequest, std::nullopt);
            target.wakePending = false;
        }
        for (const Outgoing& item : pending) {
            write(target, socket, item.data, item.flags);
        }
        if (requestedClose && !closing) {
            writeClose(target, socket, requestedClose->first, requestedClose->second);
            closing = true;
            closeDeadline = std::chrono::steady_clock::now() + kCloseTimeout;
        }
        if (closing && std::chrono::steady_clock::now() >= closeDeadline) {
            report(target, {.kind = Event::Kind::Closed, .code = kAbnormalClosure});
            return;
        }

        // Poco reads frame headers ahead into a buffer of its own, which a poll of the socket cannot see, and a wake-up from the app sends the loop back to its queue.
        const auto deadline = closing ? closeDeadline : std::chrono::steady_clock::now() + kIdleWait;
        if (socket.available() == 0 && !waitFor(target, socket, Poco::Net::PollSet::POLL_READ, deadline)) {
            continue;
        }
        int flags = 0;
        int length = 0;
        frame.resize(0);
        try {
            length = socket.receiveFrame(frame, flags);
        } catch (const Poco::Net::WebSocketException& error) {
            // Poco refuses a frame that could never fit from its header, before it reserves room for the payload.
            if (error.code() != Poco::Net::WebSocket::WS_ERR_PAYLOAD_TOO_BIG) {
                throw;
            }
            refuseMessage(target, socket, closing);
            return;
        }
        if (length < 0) {
            continue;
        }
        if (length == 0 && flags == 0) {
            report(target, {.kind = Event::Kind::Closed, .code = kAbnormalClosure});
            return;
        }

        const std::string payload(frame.begin(), frame.size());
        const int opcode = flags & Poco::Net::WebSocket::FRAME_OP_BITMASK;
        switch (opcode) {
        case Poco::Net::WebSocket::FRAME_OP_TEXT:
        case Poco::Net::WebSocket::FRAME_OP_BINARY:
            message = payload;
            messageBinary = opcode == Poco::Net::WebSocket::FRAME_OP_BINARY;
            break;
        case Poco::Net::WebSocket::FRAME_OP_CONT:
            message += payload;
            break;
        case Poco::Net::WebSocket::FRAME_OP_PING:
            write(target, socket, payload, kPongFrame);
            continue;
        case Poco::Net::WebSocket::FRAME_OP_PONG:
            report(target, {.kind = Event::Kind::Ponged, .text = payload});
            continue;
        case Poco::Net::WebSocket::FRAME_OP_CLOSE: {
            const int code = payload.size() >= 2 ? (static_cast<unsigned char>(payload[0]) << 8U) | static_cast<unsigned char>(payload[1]) : kNoStatus;
            if (!closing) {
                writeClose(target, socket, code == kNoStatus ? 1000 : code, "");
            }
            report(target, {.kind = Event::Kind::Closed, .text = payload.size() > 2 ? payload.substr(2) : std::string{}, .code = code});
            return;
        }
        default:
            continue;
        }
        if (message.size() > target.maxMessageSize) {
            refuseMessage(target, socket, closing);
            return;
        }
        if ((flags & Poco::Net::WebSocket::FRAME_FLAG_FIN) != 0) {
            report(target, {.kind = Event::Kind::Received, .text = std::exchange(message, {}), .binary = messageBinary});
        }
    }

    // A socket that the app dropped while open says goodbye before its thread ends.
    if (!closing) {
        writeClose(target, socket, Poco::Net::WebSocket::WS_ENDPOINT_GOING_AWAY, "");
    }
}

} // namespace haylen::net
