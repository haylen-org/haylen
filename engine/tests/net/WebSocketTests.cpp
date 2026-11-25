#include <gtest/gtest.h>

#include <Poco/Base64Encoder.h>
#include <Poco/Buffer.h>
#include <Poco/DigestEngine.h>
#include <Poco/Exception.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/SHA1Engine.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "haylen/net/WebSocket.hpp"
#include "net/WebSocketFixture.hpp"
#include "net/WebSocketTransport.hpp"

namespace haylen::net {

class WebSocketTest : public WebSocketFixture {
  protected:
    // Accepts one connection and never answers, like a server that hangs, and records whether the client sent something and whether it hung up.
    class SilentServer final {
      public:
        SilentServer() : listener(Poco::Net::SocketAddress("127.0.0.1", 0)), thread([this] { serve(); }) {}
        ~SilentServer() {
            done = true;
            thread.join();
        }

        SilentServer(const SilentServer&) = delete;
        SilentServer& operator=(const SilentServer&) = delete;

        [[nodiscard]] std::string getUrl(const std::string& scheme) const {
            return scheme + "://127.0.0.1:" + std::to_string(listener.address().port()) + "/";
        }
        [[nodiscard]] bool hasReceived() const noexcept {
            return received;
        }
        [[nodiscard]] bool hasHungUp() const noexcept {
            return hungUp;
        }

      private:
        static constexpr int kTickMicroseconds = 10000;

        // Whether the test still runs and the server still has time, so its thread never outlives a test that hangs.
        [[nodiscard]] bool isServing() const {
            return !done && std::chrono::steady_clock::now() < deadline;
        }

        void serve() {
            while (!listener.poll(Poco::Timespan(0, kTickMicroseconds), Poco::Net::Socket::SELECT_READ)) {
                if (!isServing()) {
                    return;
                }
            }

            Poco::Net::StreamSocket peer = listener.acceptConnection();
            std::array<char, 1024> buffer{};
            while (isServing()) {
                if (!peer.poll(Poco::Timespan(0, kTickMicroseconds), Poco::Net::Socket::SELECT_READ)) {
                    continue;
                }

                if (peer.receiveBytes(buffer.data(), static_cast<int>(buffer.size())) == 0) {
                    hungUp = true;
                    return;
                }

                received = true;
            }
        }

        Poco::Net::ServerSocket listener;
        std::atomic<bool> done = false;
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + kServeLimit;
        std::atomic<bool> received = false;
        std::atomic<bool> hungUp = false;
        std::thread thread;
    };

    // Answers connections the way a broken server does: with the status line of an HTTP error, as a server that speaks no TLS, with a reset once the client sent its request, as a server that crashes, or with an upgrade and then half a frame before it hangs up, as a server that crashes while it writes.
    class RawServer final {
      public:
        enum class Behavior : std::uint8_t {
            PlainText,
            Reset,
            BrokenFrame,
        };

        explicit RawServer(Behavior answer) : listener(Poco::Net::SocketAddress("127.0.0.1", 0)), behavior(answer), thread([this] { serve(); }) {}
        ~RawServer() {
            done = true;
            thread.join();
        }

        RawServer(const RawServer&) = delete;
        RawServer& operator=(const RawServer&) = delete;

        [[nodiscard]] std::string getUrl(const std::string& scheme) const {
            return scheme + "://127.0.0.1:" + std::to_string(listener.address().port()) + "/";
        }

      private:
        static constexpr int kTickMicroseconds = 10000;
        static constexpr std::string_view kKeyField = "Sec-WebSocket-Key: ";
        static constexpr std::string_view kAcceptSuffix = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

        // Whether the test still runs and the server still has time, so its thread never outlives a test that hangs.
        [[nodiscard]] bool isServing() const {
            return !done && std::chrono::steady_clock::now() < deadline;
        }

        // The plain answer ends the stream of the server, since a TLS client may read it as the start of a longer record and wait for the rest. The reset waits for the request, so it reaches the client while the client waits for the answer, and closing without lingering resets the connection. Half a frame announces five bytes and carries two.
        void serve() {
            while (isServing()) {
                if (!listener.poll(Poco::Timespan(0, kTickMicroseconds), Poco::Net::Socket::SELECT_READ)) {
                    continue;
                }

                Poco::Net::StreamSocket peer = listener.acceptConnection();

                // A client that already gave up the connection ends the answer, since the tests check what the client saw.
                try {
                    answer(peer);
                } catch (const Poco::Exception&) {}
            }
        }

        void answer(Poco::Net::StreamSocket& peer) const {
            switch (behavior) {
            case Behavior::PlainText:
                send(peer, "HTTP/1.1 400 Bad Request\r\n\r\n");
                peer.shutdownSend();
                drain(peer);
                break;
            case Behavior::Reset:
                (void)readRequest(peer);
                peer.setLinger(true, 0);
                peer.close();
                break;
            case Behavior::BrokenFrame:
                send(peer, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept(readRequest(peer)) + "\r\n\r\n\x81\x05he");
                break;
            }
        }

        [[nodiscard]] static bool isReadable(const Poco::Net::StreamSocket& peer) {
            return peer.poll(Poco::Timespan(0, kTickMicroseconds), Poco::Net::Socket::SELECT_READ);
        }

        static void send(Poco::Net::StreamSocket& peer, const std::string& bytes) {
            peer.sendBytes(bytes.data(), static_cast<int>(bytes.size()));
        }

        // Reads until the client hangs up, and a client that hangs up with bytes it never read resets the connection, which ends the answer.
        void drain(Poco::Net::StreamSocket& peer) const {
            std::array<char, 1024> buffer{};
            while (isServing()) {
                if (isReadable(peer) && peer.receiveBytes(buffer.data(), static_cast<int>(buffer.size())) <= 0) {
                    return;
                }
            }
        }

        // Reads the request of the client up to the blank line that ends its header.
        [[nodiscard]] std::string readRequest(Poco::Net::StreamSocket& peer) const {
            std::string request;
            std::array<char, 1024> buffer{};
            while (isServing() && request.find("\r\n\r\n") == std::string::npos) {
                if (!isReadable(peer)) {
                    continue;
                }

                const int received = peer.receiveBytes(buffer.data(), static_cast<int>(buffer.size()));
                if (received <= 0) {
                    break;
                }

                request.append(buffer.data(), static_cast<std::size_t>(received));
            }

            return request;
        }

        // Answers the key of an upgrade request the way the WebSocket protocol asks, by hand, since no WebSocket of Poco sends half a frame.
        [[nodiscard]] static std::string accept(const std::string& request) {
            const std::size_t start = request.find(kKeyField) + kKeyField.size();
            Poco::SHA1Engine engine;
            engine.update(request.substr(start, request.find("\r\n", start) - start) + std::string(kAcceptSuffix));
            const Poco::DigestEngine::Digest& digest = engine.digest();
            std::ostringstream text;
            Poco::Base64Encoder encoder(text);
            encoder.write(reinterpret_cast<const char*>(digest.data()), static_cast<std::streamsize>(digest.size()));
            encoder.close();
            return text.str();
        }

        Poco::Net::ServerSocket listener;
        Behavior behavior;
        std::atomic<bool> done = false;
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + kServeLimit;
        std::thread thread;
    };

    class FloodServer final {
      public:
        // Waits for the first message of a client and then sends it a few large binary messages before it reads anything more, while the client still sends, the way two sides that both stream do, and finally reports what the client sent.
        class Handler final : public Poco::Net::HTTPRequestHandler {
          public:
            static constexpr int kMessages = 4;
            static constexpr int kMessageSize = 8 * 1024 * 1024;

            void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
                Poco::Net::WebSocket socket(request, response);
                socket.setMaxPayloadSize(kMessageSize);
                socket.setReceiveTimeout(Poco::Timespan(kReceiveSeconds, 0));
                Poco::Buffer<char> frame(0);
                std::size_t received = 0;
                // clang-format off
                const auto receive = [&] {
                    int flags = 0;
                    frame.resize(0);
                    const int length = socket.receiveFrame(frame, flags);
                    received += static_cast<std::size_t>(std::max(length, 0));
                    return length > 0;
                };
                // clang-format on
                if (!receive()) {
                    return;
                }

                const std::string flood(kMessageSize, 'f');
                for (int message = 0; message < kMessages; ++message) {
                    socket.sendFrame(flood.data(), static_cast<int>(flood.size()), Poco::Net::WebSocket::FRAME_BINARY);
                }

                while (received < static_cast<std::size_t>(kMessages) * kMessageSize) {
                    if (!receive()) {
                        return;
                    }
                }

                const std::string done = "received " + std::to_string(received);
                socket.sendFrame(done.data(), static_cast<int>(done.size()), Poco::Net::WebSocket::FRAME_TEXT);
                (void)receive();
            }
        };

        class Factory final : public Poco::Net::HTTPRequestHandlerFactory {
          public:
            Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override {
                return new Handler();
            }
        };

        FloodServer() : listener(Poco::Net::SocketAddress("127.0.0.1", 0)), server(new Factory(), listener, new Poco::Net::HTTPServerParams()) {
            server.start();
        }
        ~FloodServer() {
            server.stopAll(true);
        }

        FloodServer(const FloodServer&) = delete;
        FloodServer& operator=(const FloodServer&) = delete;

        [[nodiscard]] std::string getUrl() const {
            return "ws://127.0.0.1:" + std::to_string(listener.address().port()) + "/flood";
        }

      private:
        Poco::Net::ServerSocket listener;
        Poco::Net::HTTPServer server;
    };
};

TEST_F(WebSocketTest, ExchangesMessagesAndCloses) {
    const EchoServer server;
    WebSocket socket(server.getUrl(), {.protocols = {"chat", "json"}});
    EXPECT_EQ(socket.getState(), WebSocket::State::Connecting);
    EXPECT_THROW(socket.send("early"), std::logic_error);
    EXPECT_THROW(socket.ping(), std::logic_error);

    std::vector<std::string> received;
    int closedCode = 0;
    std::string closedReason;
    socket.received.connect([&received](std::string_view data, bool binary) { received.push_back((binary ? "binary " : "text ") + std::string(data)); });
    // clang-format off
    socket.closed.connect([&](int code, std::string_view reason) {
        closedCode = code;
        closedReason = reason;
    });
    // clang-format on
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
    EXPECT_EQ(socket.getProtocol(), "chat");

    socket.send("hello");
    const std::vector<std::uint8_t> bytes{0, 1, 2, 255};
    socket.sendBinary(bytes);
    socket.send("fragments");
    socket.send("ping-me");
    ASSERT_TRUE(pumpUntil(socket, [&] { return received.size() == 4; }));
    EXPECT_EQ(received[0], "text hello");
    EXPECT_EQ(received[1], "binary " + std::string("\0\1\2\xFF", 4));
    EXPECT_EQ(received[2], "text hello");
    EXPECT_EQ(received[3], "text pong beat");

    // A ping comes back as a pong with the same payload.
    std::vector<std::string> pongs;
    socket.ponged.connect([&pongs](std::string_view payload) { pongs.emplace_back(payload); });
    socket.ping("heartbeat");
    socket.ping();
    ASSERT_TRUE(pumpUntil(socket, [&] { return pongs.size() == 2; }));
    EXPECT_EQ(pongs, std::vector<std::string>({"heartbeat", ""}));
    EXPECT_THROW(socket.ping(std::string(126, 'x')), std::invalid_argument);

    socket.close(1000, "done");
    EXPECT_EQ(socket.getState(), WebSocket::State::Closing);
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(closedCode, 1000);
    EXPECT_TRUE(socket.received.empty()) << "A closed socket drops its listeners.";
    socket.close();
}

TEST_F(WebSocketTest, ReportsServerClosesAndFailures) {
    const EchoServer server;
    WebSocket socket(server.getUrl());
    int closedCode = 0;
    std::string closedReason;
    // clang-format off
    socket.closed.connect([&](int code, std::string_view reason) {
        closedCode = code;
        closedReason = reason;
    });
    // clang-format on
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
    EXPECT_EQ(socket.getProtocol(), "");
    socket.send("bye");
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(closedCode, 4001);
    EXPECT_EQ(closedReason, "server bye");

    // A port without a server fails, and the socket still closes.
    WebSocket missing(refusedUrl());
    std::string failure;
    missing.failed.connect([&failure](std::string_view message) { failure = message; });
    missing.closed.connect([&closedCode](int code, std::string_view) { closedCode = code; });
    ASSERT_TRUE(pumpUntil(missing, [&] { return missing.getState() == WebSocket::State::Closed; }));
    EXPECT_FALSE(failure.empty());
    EXPECT_EQ(closedCode, 1006);

    EXPECT_THROW(WebSocket("http://127.0.0.1"), std::invalid_argument);
    EXPECT_THROW(missing.close(1234), std::invalid_argument);
    EXPECT_THROW(missing.close(1000, std::string(124, 'x')), std::invalid_argument);
    EXPECT_EQ(WebSocket::stateName(WebSocket::State::Closing), "closing");
}

TEST_F(WebSocketTest, ExplainsWhyAConnectionFailed) {
    const std::string url = refusedUrl();
    WebSocket refused(url);
    std::string failure;
    refused.failed.connect([&failure](std::string_view message) { failure = message; });
    ASSERT_TRUE(pumpUntil(refused, [&] { return refused.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(failure, "The WebSocket connection to \"" + url + "\" failed. The server refused the connection.");

    WebSocket unknown("ws://no-such-host.invalid/lobby");
    unknown.failed.connect([&failure](std::string_view message) { failure = message; });
    ASSERT_TRUE(pumpUntil(unknown, [&] { return unknown.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(failure, "The WebSocket connection to \"ws://no-such-host.invalid/lobby\" failed. The host \"no-such-host.invalid\" was not found.");

    // An address that the network libraries cannot read fails in their own words.
    WebSocket unreadable("ws://127.0.0.1:99999/lobby");
    unreadable.failed.connect([&failure](std::string_view message) { failure = message; });
    ASSERT_TRUE(pumpUntil(unreadable, [&] { return unreadable.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(failure, "The WebSocket connection to \"ws://127.0.0.1:99999/lobby\" failed. The network libraries reported \"Bad URI syntax: bad or invalid port number: 99999\".");

    const EchoServer server;
    const std::string refusing = server.getUrl("/refuse");
    WebSocket forbidden(refusing);
    forbidden.failed.connect([&failure](std::string_view message) { failure = message; });
    ASSERT_TRUE(pumpUntil(forbidden, [&] { return forbidden.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(failure, "The WebSocket connection to \"" + refusing + "\" failed. The server did not accept the WebSocket and answered \"WebSocket Exception: Cannot upgrade to WebSocket connection: Forbidden\".");

    const RawServer crashing(RawServer::Behavior::Reset);
    WebSocket abandoned(crashing.getUrl("ws"));
    abandoned.failed.connect([&failure](std::string_view message) { failure = message; });
    ASSERT_TRUE(pumpUntil(abandoned, [&] { return abandoned.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(failure, "The WebSocket connection to \"" + crashing.getUrl("ws") + "\" failed. The server reset the connection.");
}

// Every attempt of a socket that reconnects fails its TLS handshake with a server that speaks no TLS, and the second attempt shares the TLS context of the first.
TEST_F(WebSocketTest, ExplainsFailedTlsHandshakes) {
    const RawServer server(RawServer::Behavior::PlainText);
    const std::string url = server.getUrl("wss");
    WebSocket socket(url, {.reconnect = {.enabled = true, .initialDelay = 0.0F, .maxAttempts = 1}});
    std::vector<std::string> failures;
    int closedCode = 0;
    socket.failed.connect([&failures](std::string_view message) { failures.emplace_back(message); });
    socket.closed.connect([&closedCode](int code, std::string_view) { closedCode = code; });
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));

    ASSERT_EQ(failures.size(), 2U);

    for (const std::string& failure : failures) {
        EXPECT_TRUE(failure.starts_with("The WebSocket connection to \"" + url + "\" failed. The TLS handshake failed with \"")) << failure;
        EXPECT_TRUE(failure.ends_with("\".")) << failure;
    }

    EXPECT_EQ(closedCode, 1006);
}

TEST_F(WebSocketTest, ReportsAnAbnormalCloseWhenTheServerHangsUp) {
    const EchoServer server;
    WebSocket socket(server.getUrl());
    int closedCode = 0;
    std::string closedReason = "unset";
    // clang-format off
    socket.closed.connect([&](int code, std::string_view reason) {
        closedCode = code;
        closedReason = reason;
    });
    // clang-format on
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
    socket.send("drop");
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(closedCode, 1006);
    EXPECT_EQ(closedReason, "");
}

// A server that accepted the WebSocket and then broke off a frame did accept it, so the failure keeps the words of the network libraries.
TEST_F(WebSocketTest, ExplainsFramesThatEndEarly) {
    const RawServer server(RawServer::Behavior::BrokenFrame);
    const std::string url = server.getUrl("ws");
    WebSocket socket(url);
    bool opened = false;
    std::string failure;
    int closedCode = 0;
    socket.opened.connect([&opened] { opened = true; });
    socket.failed.connect([&failure](std::string_view message) { failure = message; });
    socket.closed.connect([&closedCode](int code, std::string_view) { closedCode = code; });
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_TRUE(opened);
    EXPECT_EQ(failure, "The WebSocket connection to \"" + url + "\" failed. The network libraries reported \"WebSocket Exception: Incomplete frame received\".");
    EXPECT_EQ(closedCode, 1006);
}

// The thread of a connection gives up a TLS handshake that never finishes at its own deadline, even when nothing pumps the socket.
TEST_F(WebSocketTest, TransportsEndTlsHandshakesAtTheirDeadline) {
    const SilentServer server;
    std::mutex mutex;
    std::vector<WebSocketTransport::Event> events;
    // clang-format off
    const std::unique_ptr<WebSocketTransport> transport = WebSocketTransport::open(server.getUrl("wss"), {}, 1024, 0.3F, nullptr, [&](WebSocketTransport::Event event) {
        const std::scoped_lock lock(mutex);
        events.push_back(std::move(event));
    });
    const auto reported = [&] {
        const std::scoped_lock lock(mutex);
        return events.size() == 2;
    };
    // clang-format on
    ASSERT_TRUE(waitUntil(reported, std::chrono::seconds(3)));

    const std::scoped_lock lock(mutex);
    EXPECT_EQ(events[0].kind, WebSocketTransport::Event::Kind::Failed);
    EXPECT_EQ(events[0].text, "The WebSocket connection to \"" + server.getUrl("wss") + "\" failed. The TLS handshake with \"127.0.0.1\" timed out.");
    EXPECT_EQ(events[1].kind, WebSocketTransport::Event::Kind::Closed);
    EXPECT_EQ(events[1].code, 1006);
}

TEST_F(WebSocketTest, TimesOutAttemptsThatNeverOpen) {
    SilentServer server;
    WebSocket socket(server.getUrl("ws"), {.connectTimeout = 0.3F});
    std::string failure;
    int closedCode = 0;
    socket.failed.connect([&failure](std::string_view message) { failure = message; });
    socket.closed.connect([&closedCode](int code, std::string_view) { closedCode = code; });
    const auto started = std::chrono::steady_clock::now();
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(3));
    EXPECT_EQ(failure, "The WebSocket connection to \"" + server.getUrl("ws") + "\" did not open within 0.3 seconds.");
    EXPECT_EQ(closedCode, 1006);
    EXPECT_THROW(WebSocket(server.getUrl("ws"), {.connectTimeout = 0.0F}), std::invalid_argument);
    EXPECT_THROW(WebSocket(server.getUrl("ws"), {.connectTimeout = std::numeric_limits<float>::infinity()}), std::invalid_argument);
    EXPECT_THROW(WebSocket(server.getUrl("ws"), {.connectTimeout = 3601.0F}), std::invalid_argument);
}

// A platform that drops the packets of a socket it blocks, such as Android with its Local Network Protection, records why, so the attempt fails at once with that reason instead of at its timeout.
TEST_F(WebSocketTest, FailsAtOnceWithTheReasonOfABlockedSocket) {
    SilentServer server;
    std::atomic<std::intptr_t> asked = -1;
    // clang-format off
    WebSocket socket(server.getUrl("ws"), {.connectTimeout = 30.0F, .blockedReason = [&asked](std::intptr_t descriptor) {
        asked = descriptor;
        return std::string("The platform blocked the socket.");
    }});
    // clang-format on
    std::string failure;
    socket.failed.connect([&failure](std::string_view message) { failure = message; });
    const auto started = std::chrono::steady_clock::now();
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(3));
    EXPECT_EQ(failure, "The WebSocket connection to \"" + server.getUrl("ws") + "\" failed. The platform blocked the socket.");
    EXPECT_GE(asked.load(), 0);
}

TEST_F(WebSocketTest, GivesUpAnUpgradeRequestWithTheConnectTimeout) {
    const SilentServer server;
    WebSocket socket(server.getUrl("ws"), {.connectTimeout = 0.3F});
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));

    // The thread of the attempt stops waiting for the answer to its upgrade request when the connect timeout runs out.
    EXPECT_TRUE(waitUntil([&] { return server.hasHungUp(); }, std::chrono::seconds(3)));
}

TEST_F(WebSocketTest, ASocketClosedWhileConnectingNeverOpens) {
    const EchoServer server;
    WebSocket socket(server.getUrl());
    bool opened = false;
    int closedCode = 0;
    socket.opened.connect([&opened] { opened = true; });
    socket.closed.connect([&closedCode](int code, std::string_view) { closedCode = code; });
    socket.close(4000, "changed my mind");
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_FALSE(opened);
    EXPECT_NE(closedCode, 0);
    EXPECT_EQ(socket.getProtocol(), "");
}

TEST_F(WebSocketTest, CountsTheBytesThatWaitToBeWritten) {
    const EchoServer server;
    WebSocket socket(server.getUrl());
    std::size_t echoed = 0;
    socket.received.connect([&echoed](std::string_view data, bool) { echoed += data.size(); });
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
    EXPECT_EQ(socket.getBufferedAmount(), 0U);

    const std::string chunk(1024 * 1024, 'c');
    const std::size_t total = 8 * chunk.size();
    for (int index = 0; index < 8; ++index) {
        socket.send(chunk);
    }

    EXPECT_EQ(socket.getBufferedAmount(), total);

    // The connection writes on its own thread, and what it wrote leaves the count only when the socket pumps, as in browsers, however fast the network takes it.
    ASSERT_TRUE(waitUntil([&] { return server.getReceivedBytes() == total; }));
    EXPECT_EQ(socket.getBufferedAmount(), total);
    socket.send("one more");
    EXPECT_EQ(socket.getBufferedAmount(), total + 8);

    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getBufferedAmount() == 0 && echoed == total + 8; }));
    socket.close();
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(socket.getBufferedAmount(), 0U);
}

TEST_F(WebSocketTest, KeepsReadingWhileItWaitsToWrite) {
    const FloodServer server;
    WebSocket socket(server.getUrl(), {.maxMessageSize = FloodServer::Handler::kMessageSize});
    int floods = 0;
    std::string report;
    // clang-format off
    socket.received.connect([&](std::string_view data, bool binary) {
        if (binary) {
            ++floods;
        } else {
            report = data;
        }
    });
    // clang-format on
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
    const std::vector<std::uint8_t> message(FloodServer::Handler::kMessageSize, 'm');
    for (int index = 0; index < FloodServer::Handler::kMessages; ++index) {
        socket.sendBinary(message);
    }

    ASSERT_TRUE(pumpUntil(socket, [&] { return !report.empty(); })) << "Both sides waited for the other to read.";
    EXPECT_EQ(floods, FloodServer::Handler::kMessages);
    EXPECT_EQ(report, "received " + std::to_string(static_cast<std::size_t>(FloodServer::Handler::kMessages) * FloodServer::Handler::kMessageSize));
}

TEST_F(WebSocketTest, DisconnectListenersSeeTheStateThatFollows) {
    const EchoServer server;
    WebSocket socket(server.getUrl(), {.reconnect = {.enabled = true}});
    std::string seen;
    int closedCode = 0;
    // clang-format off
    socket.disconnected.connect([&](int, std::string_view) {
        seen = WebSocket::stateName(socket.getState());
        EXPECT_THROW(socket.send("late"), std::logic_error);
        EXPECT_THROW(socket.sendBinary({}), std::logic_error);
        EXPECT_THROW(socket.ping(), std::logic_error);
        socket.close(4000, "leaving");
    });
    // clang-format on
    socket.closed.connect([&closedCode](int code, std::string_view) { closedCode = code; });

    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
    socket.send("bye");
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(seen, "reconnecting");
    EXPECT_EQ(closedCode, 4000) << "The close of the listener ends the socket instead of the next attempt.";
    EXPECT_EQ(socket.getAttempt(), 0);
}

TEST_F(WebSocketTest, RefusesMessagesLargerThanTheLimit) {
    const EchoServer server;

    // The message `hello` comes back in one frame, `fragments` grows past the limit with its second frame, and the long message is larger than any frame the socket reads.
    for (const std::string& request : {std::string("hello"), std::string("fragments"), std::string(200, 'x')}) {
        WebSocket socket(server.getUrl(), {.maxMessageSize = 4});
        std::vector<std::string> received;
        std::string failure;
        int closedCode = 0;
        socket.received.connect([&received](std::string_view data, bool) { received.emplace_back(data); });
        socket.failed.connect([&failure](std::string_view message) { failure = message; });
        socket.closed.connect([&closedCode](int code, std::string_view) { closedCode = code; });

        ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
        socket.send("abcd");
        socket.send(request);
        ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
        EXPECT_EQ(received, (std::vector<std::string>{"abcd"}));
        EXPECT_EQ(failure, "The server sent a WebSocket message larger than the maximum of 4 bytes.");
        EXPECT_EQ(closedCode, 1009);
    }

    EXPECT_THROW(WebSocket(server.getUrl(), {.maxMessageSize = 0}), std::invalid_argument);
    EXPECT_THROW(WebSocket(server.getUrl(), {.maxMessageSize = 2147483648U}), std::invalid_argument);
}

TEST_F(WebSocketTest, DestroyingNeverWaitsForAConnectionThatHangs) {
    const SilentServer server;
    auto socket = std::make_unique<WebSocket>(server.getUrl("ws"));
    ASSERT_TRUE(waitUntil([&] { return server.hasReceived(); })) << "The upgrade request waits for an answer that never comes.";

    const auto start = std::chrono::steady_clock::now();
    socket.reset();
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(100));
}

TEST_F(WebSocketTest, AbandonsATlsHandshakeAtOnce) {
    const SilentServer server;
    auto socket = std::make_unique<WebSocket>(server.getUrl("wss"));
    ASSERT_TRUE(waitUntil([&] { return server.hasReceived(); })) << "The client hello waits for a server hello that never comes.";

    // The thread leaves the handshake as soon as the socket is gone and closes its connection, long before the handshake would time out.
    socket.reset();
    EXPECT_TRUE(waitUntil([&] { return server.hasHungUp(); }, std::chrono::seconds(2)));
}

TEST_F(WebSocketTest, SleepingConnectionsWakeForTheApp) {
    const EchoServer server;
    WebSocket socket(server.getUrl());
    std::vector<std::string> received;
    socket.received.connect([&received](std::string_view data, bool) { received.emplace_back(data); });
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));

    // The connection thread sleeps until the socket or the app has something for it, so a send after a quiet moment goes out at once.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto start = std::chrono::steady_clock::now();
    socket.send("awake");
    ASSERT_TRUE(pumpUntil(socket, [&] { return received.size() == 1; }));
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
    socket.close();
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
}

TEST_F(WebSocketTest, BacksOffBetweenReconnectAttempts) {
    const WebSocket::Reconnect backoff{.enabled = true, .initialDelay = 1.0F, .maxDelay = 4.0F, .multiplier = 2.0F, .jitter = 0.0F, .maxAttempts = 3};
    EXPECT_EQ(WebSocket::getBackoff(backoff, 1), 1.0F);
    EXPECT_EQ(WebSocket::getBackoff(backoff, 2), 2.0F);
    EXPECT_EQ(WebSocket::getBackoff(backoff, 3), 4.0F);
    EXPECT_EQ(WebSocket::getBackoff(backoff, 9), 4.0F);

    // The fake clock decides when each attempt starts, and every attempt fails because nothing listens.
    double now = 0.0;
    WebSocket socket(refusedUrl(), {.reconnect = backoff, .clock = [&now] { return now; }});
    std::vector<std::string> log;
    int failures = 0;
    socket.failed.connect([&failures](std::string_view) { ++failures; });
    socket.reconnecting.connect([&log](int attempt, float delay) { log.push_back("reconnecting " + std::to_string(attempt) + " after " + std::to_string(static_cast<int>(delay))); });
    socket.closed.connect([&log](int code, std::string_view) { log.push_back("closed " + std::to_string(code)); });
    socket.disconnected.connect([&log](int, std::string_view) { log.push_back("disconnected"); });

    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Reconnecting; }));
    EXPECT_EQ(socket.getAttempt(), 1);
    now = 0.9;
    socket.pump();
    EXPECT_EQ(socket.getState(), WebSocket::State::Reconnecting) << "The first attempt waits a whole second.";
    now = 1.0;
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getAttempt() == 2; }));
    now = 3.0;
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getAttempt() == 3; }));
    now = 7.0;
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(log, (std::vector<std::string>{"reconnecting 1 after 1", "reconnecting 2 after 2", "reconnecting 3 after 4", "closed 1006"}));
    EXPECT_EQ(failures, 4);
    EXPECT_EQ(WebSocket::stateName(WebSocket::State::Reconnecting), "reconnecting");
}

TEST_F(WebSocketTest, ReconnectsAfterTheServerDropsAndStopsWhenClosed) {
    const EchoServer server;
    double now = 0.0;
    WebSocket socket(server.getUrl(), {.reconnect = {.enabled = true, .initialDelay = 2.0F, .jitter = 0.5F}, .clock = [&now] { return now; }, .seed = 7});
    std::vector<std::string> log;
    float wait = 0.0F;
    socket.opened.connect([&log] { log.emplace_back("open"); });
    socket.disconnected.connect([&log](int code, std::string_view reason) { log.push_back("disconnected " + std::to_string(code) + " " + std::string(reason)); });
    // clang-format off
    socket.reconnecting.connect([&](int attempt, float delay) {
        log.push_back("reconnecting " + std::to_string(attempt));
        wait = delay;
    });
    // clang-format on
    socket.closed.connect([&log](int code, std::string_view) { log.push_back("closed " + std::to_string(code)); });

    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
    socket.send("bye");
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Reconnecting; }));
    EXPECT_GE(wait, 1.0F) << "Jitter shortens the wait by half at most.";
    EXPECT_LE(wait, 2.0F);
    EXPECT_THROW(socket.send("early"), std::logic_error);
    now += static_cast<double>(wait);
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
    EXPECT_EQ(socket.getAttempt(), 0);

    socket.close(4000, "done");
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Closed; }));
    EXPECT_EQ(log, (std::vector<std::string>{"open", "disconnected 4001 server bye", "reconnecting 1", "open", "disconnected 1000 ", "closed 1000"}));

    // Closing while the socket waits for its next attempt ends it with the code of the app.
    WebSocket waiting(refusedUrl(), {.reconnect = {.enabled = true, .initialDelay = 60.0F, .maxDelay = 60.0F}, .clock = [&now] { return now; }});
    int closedCode = 0;
    waiting.closed.connect([&closedCode](int code, std::string_view) { closedCode = code; });
    ASSERT_TRUE(pumpUntil(waiting, [&] { return waiting.getState() == WebSocket::State::Reconnecting; }));
    waiting.close(4002, "leaving");
    EXPECT_EQ(waiting.getState(), WebSocket::State::Closing);
    waiting.pump();
    EXPECT_EQ(waiting.getState(), WebSocket::State::Closed);
    EXPECT_EQ(closedCode, 4002);

    EXPECT_THROW(WebSocket(server.getUrl(), {.reconnect = {.enabled = true, .multiplier = 0.5F}}), std::invalid_argument);
    EXPECT_THROW(WebSocket(server.getUrl(), {.reconnect = {.enabled = true, .initialDelay = 5.0F, .maxDelay = 1.0F}}), std::invalid_argument);
    EXPECT_THROW(WebSocket(server.getUrl(), {.reconnect = {.enabled = true, .jitter = 2.0F}}), std::invalid_argument);

    // Delays that are not numbers would leave the socket waiting forever.
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(WebSocket(server.getUrl(), {.reconnect = {.enabled = true, .initialDelay = notANumber}}), std::invalid_argument);
    EXPECT_THROW(WebSocket(server.getUrl(), {.reconnect = {.enabled = true, .maxDelay = notANumber}}), std::invalid_argument);
    EXPECT_THROW(WebSocket(server.getUrl(), {.reconnect = {.enabled = true, .multiplier = notANumber}}), std::invalid_argument);
    EXPECT_THROW(WebSocket(server.getUrl(), {.reconnect = {.enabled = true, .jitter = notANumber}}), std::invalid_argument);
    EXPECT_THROW(WebSocket(server.getUrl(), {.reconnect = {.enabled = true, .maxDelay = std::numeric_limits<float>::infinity()}}), std::invalid_argument);
}

} // namespace haylen::net
