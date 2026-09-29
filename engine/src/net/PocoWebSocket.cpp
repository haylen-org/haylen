#include "net/PocoWebSocket.hpp"

#include <Poco/Buffer.h>
#include <Poco/Crypto/OpenSSLInitializer.h>
#include <Poco/Exception.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/RejectCertificateHandler.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/URI.h>

#include <stdexcept>

#include "varn/tls/CaBundle.h"

namespace haylen::net {

std::unique_ptr<WebSocketTransport> WebSocketTransport::open(const std::string& url, const std::vector<std::string>& protocols, Sink sink) {
    return std::make_unique<PocoWebSocket>(url, protocols, std::move(sink));
}

PocoWebSocket::PocoWebSocket(std::string url, std::vector<std::string> protocols, Sink target) : sink(std::move(target)) {
    thread = std::thread([this, address = std::move(url), names = std::move(protocols)] { run(address, names); });
}

PocoWebSocket::~PocoWebSocket() {
    stopping = true;
    thread.join();
}

void PocoWebSocket::send(std::string data, bool binary) {
    const std::scoped_lock lock(mutex);
    outgoing.push_back({std::move(data), binary ? Poco::Net::WebSocket::FRAME_BINARY : Poco::Net::WebSocket::FRAME_TEXT});
}

void PocoWebSocket::ping(std::string payload) {
    const std::scoped_lock lock(mutex);
    outgoing.push_back({std::move(payload), kPingFrame});
}

void PocoWebSocket::close(int code, std::string reason) {
    const std::scoped_lock lock(mutex);
    closeRequest = std::make_pair(code, std::move(reason));
}

void PocoWebSocket::report(Event event) {
    if (!stopping) {
        sink(std::move(event));
    }
}

Poco::Net::Context::Ptr PocoWebSocket::getClientContext() {
    static const Poco::Crypto::OpenSSLInitializer initializer;
    static std::once_flag once;
    // clang-format off
    std::call_once(once, [] {
        Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> handler(new Poco::Net::RejectCertificateHandler(false));
        Poco::Net::SSLManager::instance().initializeClient(nullptr, handler, nullptr);
    });
    // clang-format on

#if defined(_WIN32)
    static const Poco::Net::Context::Ptr context = new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "", Poco::Net::Context::VERIFY_STRICT, Poco::Net::Context::OPT_DEFAULTS, Poco::Net::Context::CERT_STORE_ROOT);
#else
    const std::string& bundle = varn::tls::CaBundle::resolve();
    if (bundle.empty()) {
        throw std::runtime_error("No trust store was found to check the certificate of a wss:// server: " + varn::tls::CaBundle::describeSearch() + ".");
    }
    static const Poco::Net::Context::Ptr context = new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "", "", bundle, Poco::Net::Context::VERIFY_STRICT, 9, true, "DEFAULT@SECLEVEL=2");
#endif
    return context;
}

std::unique_ptr<Poco::Net::WebSocket> PocoWebSocket::connect(const std::string& url, const std::vector<std::string>& protocols, std::string& protocol) {
    const Poco::URI uri(url);
    const bool secure = uri.getScheme() == "wss";
    std::unique_ptr<Poco::Net::HTTPClientSession> session;
    if (secure) {
        session = std::make_unique<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort(), getClientContext());
    } else {
        session = std::make_unique<Poco::Net::HTTPClientSession>(uri.getHost(), uri.getPort());
    }
    session->setTimeout(Poco::Timespan(kConnectSeconds, 0));

    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, uri.getPathAndQuery().empty() ? "/" : uri.getPathAndQuery(), Poco::Net::HTTPMessage::HTTP_1_1);
    if (!protocols.empty()) {
        std::string list;
        for (const std::string& name : protocols) {
            list += (list.empty() ? "" : ", ") + name;
        }
        request.set("Sec-WebSocket-Protocol", list);
    }
    Poco::Net::HTTPResponse response;
    auto socket = std::make_unique<Poco::Net::WebSocket>(*session, request, response);
    // Blocking reads would wait for a full header's worth of bytes even when a small frame already arrived.
    socket->setBlocking(false);
    protocol = response.get("Sec-WebSocket-Protocol", "");
    return socket;
}

void PocoWebSocket::run(const std::string& url, const std::vector<std::string>& protocols) {
    std::unique_ptr<Poco::Net::WebSocket> socket;
    try {
        std::string protocol;
        socket = connect(url, protocols, protocol);
        report({.kind = Event::Kind::Opened, .text = std::move(protocol)});
        serve(*socket);
    } catch (const Poco::Exception& error) {
        report({.kind = Event::Kind::Failed, .text = error.displayText()});
        report({.kind = Event::Kind::Closed, .code = kAbnormalClosure});
    } catch (const std::exception& error) {
        report({.kind = Event::Kind::Failed, .text = error.what()});
        report({.kind = Event::Kind::Closed, .code = kAbnormalClosure});
    }
}

void PocoWebSocket::write(Poco::Net::WebSocket& socket, std::string_view data, int flags) const {
    while (socket.sendFrame(data.data(), static_cast<int>(data.size()), flags) < 0 && !stopping) {
        socket.poll(Poco::Timespan(0, kPollMicroseconds), Poco::Net::Socket::SELECT_WRITE);
    }
}

void PocoWebSocket::writeClose(Poco::Net::WebSocket& socket, int code, std::string_view reason) const {
    std::string payload{static_cast<char>((code >> 8) & 0xFF), static_cast<char>(code & 0xFF)};
    payload += reason;
    write(socket, payload, kCloseFrame);
}

void PocoWebSocket::serve(Poco::Net::WebSocket& socket) {
    Poco::Buffer<char> frame(0);
    std::string message;
    bool messageBinary = false;
    bool closing = false;
    std::chrono::steady_clock::time_point closeDeadline;

    while (!stopping) {
        std::vector<Outgoing> pending;
        std::optional<std::pair<int, std::string>> requestedClose;
        {
            const std::scoped_lock lock(mutex);
            pending.swap(outgoing);
            requestedClose = std::exchange(closeRequest, std::nullopt);
        }
        for (const Outgoing& item : pending) {
            write(socket, item.data, item.flags);
        }
        if (requestedClose && !closing) {
            writeClose(socket, requestedClose->first, requestedClose->second);
            closing = true;
            closeDeadline = std::chrono::steady_clock::now() + kCloseTimeout;
        }
        if (closing && std::chrono::steady_clock::now() >= closeDeadline) {
            report({.kind = Event::Kind::Closed, .code = kAbnormalClosure});
            return;
        }

        // Poco reads frame headers ahead into a buffer of its own, which a poll of the socket cannot see.
        if (socket.available() == 0 && !socket.poll(Poco::Timespan(0, kPollMicroseconds), Poco::Net::Socket::SELECT_READ)) {
            continue;
        }
        int flags = 0;
        frame.resize(0);
        const int length = socket.receiveFrame(frame, flags);
        if (length < 0) {
            continue;
        }
        if (length == 0 && flags == 0) {
            report({.kind = Event::Kind::Closed, .code = kAbnormalClosure});
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
            write(socket, payload, kPongFrame);
            continue;
        case Poco::Net::WebSocket::FRAME_OP_PONG:
            report({.kind = Event::Kind::Ponged, .text = payload});
            continue;
        case Poco::Net::WebSocket::FRAME_OP_CLOSE: {
            const int code = payload.size() >= 2 ? (static_cast<unsigned char>(payload[0]) << 8U) | static_cast<unsigned char>(payload[1]) : kNoStatus;
            if (!closing) {
                writeClose(socket, code == kNoStatus ? 1000 : code, "");
            }
            report({.kind = Event::Kind::Closed, .text = payload.size() > 2 ? payload.substr(2) : std::string{}, .code = code});
            return;
        }
        default:
            continue;
        }
        if ((flags & Poco::Net::WebSocket::FRAME_FLAG_FIN) != 0) {
            report({.kind = Event::Kind::Received, .text = std::exchange(message, {}), .binary = messageBinary});
        }
    }

    // A socket dropped while open says goodbye before its thread ends.
    if (!closing) {
        writeClose(socket, Poco::Net::WebSocket::WS_ENDPOINT_GOING_AWAY, "");
    }
}

} // namespace haylen::net
