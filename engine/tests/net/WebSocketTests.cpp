#include <gtest/gtest.h>

#include <Poco/Buffer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/WebSocket.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/net/WebSocket.hpp"
#include "haylen/plugins/NetPlugin.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::net {

namespace {

// Echoes every message, answers pings with pongs and understands a few commands: `fragments` answers in two frames, `ping-me` pings the client and reports its pong, and `bye` closes from the server side.
class EchoHandler final : public Poco::Net::HTTPRequestHandler {
  public:
    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
        if (request.get("Sec-WebSocket-Protocol", "").find("chat") != std::string::npos) {
            response.set("Sec-WebSocket-Protocol", "chat");
        }
        Poco::Net::WebSocket socket(request, response);
        Poco::Buffer<char> frame(0);
        while (true) {
            int flags = 0;
            frame.resize(0);
            const int length = socket.receiveFrame(frame, flags);
            const int opcode = flags & Poco::Net::WebSocket::FRAME_OP_BITMASK;
            const std::string text(frame.begin(), frame.size());
            if ((length == 0 && flags == 0) || opcode == Poco::Net::WebSocket::FRAME_OP_CLOSE) {
                socket.shutdown(Poco::Net::WebSocket::WS_NORMAL_CLOSE, "");
                return;
            }
            if (opcode == Poco::Net::WebSocket::FRAME_OP_PONG) {
                send(socket, "pong " + text);
            } else if (opcode == Poco::Net::WebSocket::FRAME_OP_PING) {
                socket.sendFrame(frame.begin(), length, static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PONG));
            } else if (text == "fragments") {
                socket.sendFrame("hel", 3, Poco::Net::WebSocket::FRAME_OP_TEXT);
                socket.sendFrame("lo", 2, static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_CONT));
            } else if (text == "ping-me") {
                socket.sendFrame("beat", 4, static_cast<int>(Poco::Net::WebSocket::FRAME_FLAG_FIN) | static_cast<int>(Poco::Net::WebSocket::FRAME_OP_PING));
            } else if (text == "bye") {
                socket.shutdown(4001, "server bye");
                socket.receiveFrame(frame, flags);
                return;
            } else {
                socket.sendFrame(frame.begin(), length, opcode == Poco::Net::WebSocket::FRAME_OP_BINARY ? Poco::Net::WebSocket::FRAME_BINARY : Poco::Net::WebSocket::FRAME_TEXT);
            }
        }
    }

  private:
    static void send(Poco::Net::WebSocket& socket, const std::string& text) {
        socket.sendFrame(text.data(), static_cast<int>(text.size()), Poco::Net::WebSocket::FRAME_TEXT);
    }
};

class EchoFactory final : public Poco::Net::HTTPRequestHandlerFactory {
  public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override {
        return new EchoHandler();
    }
};

class EchoServer final {
  public:
    EchoServer() : listener(Poco::Net::SocketAddress("127.0.0.1", 0)), server(new EchoFactory(), listener, new Poco::Net::HTTPServerParams()) {
        server.start();
    }
    ~EchoServer() {
        server.stopAll(true);
    }

    EchoServer(const EchoServer&) = delete;
    EchoServer& operator=(const EchoServer&) = delete;

    [[nodiscard]] std::string getUrl() const {
        return "ws://127.0.0.1:" + std::to_string(listener.address().port()) + "/echo";
    }

  private:
    Poco::Net::ServerSocket listener;
    Poco::Net::HTTPServer server;
};

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

    void serve() {
        while (!listener.poll(Poco::Timespan(0, kTickMicroseconds), Poco::Net::Socket::SELECT_READ)) {
            if (done) {
                return;
            }
        }
        Poco::Net::StreamSocket peer = listener.acceptConnection();
        std::array<char, 1024> buffer{};
        while (!done) {
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
    std::atomic<bool> received = false;
    std::atomic<bool> hungUp = false;
    std::thread thread;
};

// Waits for the first message of a client and then sends it a few large binary messages before it reads anything more, while the client still sends, the way two sides that both stream do, and finally reports what the client sent.
class FloodHandler final : public Poco::Net::HTTPRequestHandler {
  public:
    static constexpr int kMessages = 4;
    static constexpr int kMessageSize = 8 * 1024 * 1024;

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
        Poco::Net::WebSocket socket(request, response);
        socket.setMaxPayloadSize(kMessageSize);
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

class FloodFactory final : public Poco::Net::HTTPRequestHandlerFactory {
  public:
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override {
        return new FloodHandler();
    }
};

class FloodServer final {
  public:
    FloodServer() : listener(Poco::Net::SocketAddress("127.0.0.1", 0)), server(new FloodFactory(), listener, new Poco::Net::HTTPServerParams()) {
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

class WebSocketTest : public ::testing::Test {
  protected:
    // Pumps until the condition holds, since the connection works on a thread of its own.
    static bool pumpUntil(WebSocket& socket, const std::function<bool()>& done) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            socket.pump();
            if (done()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    }

    static bool waitUntil(const std::function<bool()>& done, std::chrono::milliseconds timeout = std::chrono::seconds(10)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return done();
    }

    // Returns an address where nothing listens, so every connection attempt fails at once.
    static std::string refusedUrl() {
        Poco::Net::ServerSocket unused(Poco::Net::SocketAddress("127.0.0.1", 0));
        const std::string url = "ws://127.0.0.1:" + std::to_string(unused.address().port());
        unused.close();
        return url;
    }
};

class NetPluginTest : public WebSocketTest {};

class NetLuaTest : public WebSocketTest {};

} // namespace

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
}

TEST_F(WebSocketTest, CountsTheBytesThatWaitToBeWritten) {
    const EchoServer server;
    WebSocket socket(server.getUrl());
    std::size_t echoed = 0;
    socket.received.connect([&echoed](std::string_view data, bool) { echoed += data.size(); });
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getState() == WebSocket::State::Open; }));
    EXPECT_EQ(socket.getBufferedAmount(), 0U);

    const std::string chunk(1024 * 1024, 'c');
    for (int index = 0; index < 8; ++index) {
        socket.send(chunk);
    }
    EXPECT_GT(socket.getBufferedAmount(), 0U);
    ASSERT_TRUE(pumpUntil(socket, [&] { return socket.getBufferedAmount() == 0 && echoed == 8 * chunk.size(); }));
}

TEST_F(WebSocketTest, KeepsReadingWhileItWaitsToWrite) {
    const FloodServer server;
    WebSocket socket(server.getUrl(), {.maxMessageSize = FloodHandler::kMessageSize});
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
    const std::vector<std::uint8_t> message(FloodHandler::kMessageSize, 'm');
    for (int index = 0; index < FloodHandler::kMessages; ++index) {
        socket.sendBinary(message);
    }
    ASSERT_TRUE(pumpUntil(socket, [&] { return !report.empty(); })) << "Both sides waited for the other to read.";
    EXPECT_EQ(floods, FloodHandler::kMessages);
    EXPECT_EQ(report, "received " + std::to_string(static_cast<std::size_t>(FloodHandler::kMessages) * FloodHandler::kMessageSize));
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

TEST_F(NetLuaTest, TalksThroughWebSocketsFromLua) {
    const EchoServer server;
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        net = require('haylen.net')
        log = {}
        local socket = net.connectWebSocket(')" + server.getUrl() + R"(', {protocols = {'chat'}})
        socket:on('open', function() log[#log + 1] = 'open ' .. socket.protocol socket:send('hi') socket:ping('beat') end)
        socket:on('pong', function(payload) log[#log + 1] = 'pong ' .. payload socket:sendBinary('\0\1') end)
        socket:on('message', function(data, binary) log[#log + 1] = (binary and 'binary ' .. #data or 'text ' .. data) if binary then socket:close(4000, 'thanks') end end)
        socket:on('close', function(code, reason) log[#log + 1] = 'close ' .. code end)
        state = socket.state
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return state .. ' ' .. net.openSocketCount()"), "connecting 1");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (fixture.lua("return #log") != "5" && std::chrono::steady_clock::now() < deadline) {
        fixture.frames(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "open chat, text hi, pong beat, binary 2, close 1000");
    EXPECT_EQ(fixture.lua("return net.openSocketCount()"), "0");
    EXPECT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();

    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "'):on('data', print)").find("Unknown WebSocket event \"data\""), std::string::npos);
    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "'):ping()").find("is not open"), std::string::npos);
    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "', {protocol = 'x'})").find("Unknown option \"protocol\""), std::string::npos);
    EXPECT_NE(fixture.lua("net.connectWebSocket('ftp://nowhere')").find("\"ws://\" or \"wss://\""), std::string::npos);
    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "', {maxMessageSize = 0})").find("maximum message size between 1 and 2147483647 bytes"), std::string::npos);
}

TEST_F(NetLuaTest, ReportsFailedConnectionsWithTheirAddress) {
    const std::string url = refusedUrl();
    test::EngineFixture fixture;
    fixture.runLua("local socket = require('haylen.net').connectWebSocket('" + url + "') socket:on('error', function(message) failure = socket.url .. ' ' .. tostring(#message > 0) end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(failure)") != "nil"; }));
    EXPECT_EQ(fixture.lua("return failure"), url + " true");
}

TEST_F(NetLuaTest, FailuresEndWithWhatTheProjectLacksForTheNetwork) {
    const std::string url = refusedUrl();
    const std::string requirement = "The app does not declare the permission \"android.permission.INTERNET\".";
    test::EngineFixture fixture;
    fixture.host().setNetworkRequirement(requirement);
    fixture.restart();
    fixture.runLua("require('haylen.net').connectWebSocket('" + url + "'):on('error', function(message) failure = message end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(failure)") != "nil"; }));
    const std::string failure = fixture.lua("return failure");
    EXPECT_TRUE(failure.ends_with(". " + requirement)) << failure;
    EXPECT_GT(failure.size(), requirement.size() + 2);
}

TEST_F(NetLuaTest, EndsListenersWithTheirOwner) {
    const std::string url = refusedUrl();
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        heard = {}
        local socket = require('haylen.net').connectWebSocket(')" + url + R"(')
        local holder = {}
        socket:on('error', function() heard[#heard + 1] = 'owned' end, {owner = holder})
        socket:on('error', function() heard[#heard + 1] = 'free' end)
        holder = nil
        collectgarbage()
        collectgarbage()
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #heard") != "0"; }));
    EXPECT_EQ(fixture.lua("return table.concat(heard, ' ')"), "free");
    EXPECT_NE(fixture.lua("require('haylen.net').connectWebSocket('" + url + "'):on('open', function() end, {weak = true})").find("Unknown option \"weak\""), std::string::npos);
}

TEST_F(NetLuaTest, ClosesSocketsWithTheirOwnerAndReadsTheirOptions) {
    const EchoServer server;
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        net = require('haylen.net')
        local holder = {}
        owned = net.connectWebSocket(')" + server.getUrl() + R"(', {owner = holder, connectTimeout = 5})
        kept = net.connectWebSocket(')" + server.getUrl() + R"(')
        release = function() holder = nil collectgarbage() collectgarbage() end
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return owned.state .. kept.state") == "openopen"; }));
    EXPECT_EQ(fixture.lua("return owned.bufferedAmount"), "0");
    fixture.runLua("release()");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return owned.state") == "closed"; }));
    EXPECT_EQ(fixture.lua("return kept.state"), "open");
    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "', {connectTimeout = 0})").find("A WebSocket needs a connect timeout above 0 seconds."), std::string::npos);
    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "', {owner = 5})").find("An owner must be a table or a userdata, not number."), std::string::npos);
}

TEST_F(NetLuaTest, KeepsListenersAwayFromEndedAndOversizedConnections) {
    const EchoServer server;
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local net = require('haylen.net')
        log = {}
        local dropped = net.connectWebSocket(')" + server.getUrl() + R"(')
        dropped:on('open', function() dropped:send('bye') end)
        dropped:on('disconnect', function()
            local sent, message = pcall(dropped.send, dropped, 'late')
            log[#log + 1] = 'disconnect ' .. dropped.state .. ' ' .. tostring(sent) .. ' ' .. tostring(message:find('is not open') ~= nil)
            dropped:close()
        end)
        dropped:on('close', function(code) log[#log + 1] = 'dropped close ' .. code end)

        local limited = net.connectWebSocket(')" + server.getUrl() + R"(', {maxMessageSize = 4})
        limited:on('open', function() limited:send('hello') end)
        limited:on('error', function(message) log[#log + 1] = 'limited error ' .. tostring(message:find('maximum of 4 bytes') ~= nil) end)
        limited:on('close', function(code) log[#log + 1] = 'limited close ' .. code end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #log") == "4"; }));
    EXPECT_EQ(fixture.lua("table.sort(log) return table.concat(log, ', ')"), "disconnect closed false true, dropped close 4001, limited close 1009, limited error true");
    EXPECT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
}

TEST_F(NetPluginTest, ClosesOpenSocketsWhenTheAppStops) {
    const EchoServer server;
    std::shared_ptr<WebSocket> socket;
    {
        test::EngineFixture fixture;
        socket = fixture.engine().getPlugin<plugins::NetPlugin>().connectWebSocket(server.getUrl());
        EXPECT_EQ(fixture.engine().getPlugin<plugins::NetPlugin>().getOpenSocketCount(), 1U);
        ASSERT_TRUE(pumpUntil(*socket, [&] { return socket->getState() == WebSocket::State::Open; }));
        socket->opened.connect([] {});
    }
    EXPECT_TRUE(socket->opened.empty());
    EXPECT_EQ(socket->getState(), WebSocket::State::Closing);
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
}

TEST_F(NetPluginTest, PublishesConnectionEvents) {
    const EchoServer server;
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local net = require('haylen.net')
        local events = require('haylen.events')
        log = {}
        events.on('webSocketConnected', function(data) log[#log + 1] = 'connected ' .. tostring(data.url == url) end)
        events.on('webSocketDisconnected', function(data) log[#log + 1] = 'disconnected ' .. data.code .. ' ' .. data.reason end)
        events.on('webSocketReconnecting', function(data) log[#log + 1] = 'reconnecting ' .. data.attempt .. ' ' .. tostring(data.delay <= 0.01) end)
        url = ')" + server.getUrl() + R"('
        socket = net.connectWebSocket(url, {reconnect = {initialDelay = 0.01, maxDelay = 0.05, multiplier = 3, jitter = 0, maxAttempts = 4}})
        socket:on('open', function() if socket.attempt == 0 and not said then said = true socket:send('bye') end end)
        socket:on('reconnecting', function(attempt, delay) log[#log + 1] = 'socket reconnecting ' .. attempt end)
        socket:on('disconnect', function(code) log[#log + 1] = 'socket disconnect ' .. code end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #log") == "6"; }));
    EXPECT_EQ(fixture.lua("return table.concat(log, ', ')"), "connected true, disconnected 4001 server bye, socket disconnect 4001, reconnecting 1 true, socket reconnecting 1, connected true");
    EXPECT_EQ(fixture.lua("return socket.state .. ' ' .. socket.attempt"), "open 0");

    // A socket gives up after its attempts, and `true` picks the default backoff.
    fixture.runLua("failing = require('haylen.net').connectWebSocket('" + refusedUrl() + "', {reconnect = {initialDelay = 0, maxAttempts = 2}}) failing:on('close', function(code) gaveUp = code end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(gaveUp)") == "1006"; }));
    EXPECT_EQ(fixture.lua("return require('haylen.net').connectWebSocket('" + server.getUrl() + "', {reconnect = true}).state"), "connecting");
    EXPECT_NE(fixture.lua("require('haylen.net').connectWebSocket('" + server.getUrl() + "', {reconnect = {delay = 1}})").find("Unknown option \"delay\""), std::string::npos);
    EXPECT_NE(fixture.lua("require('haylen.net').connectWebSocket('" + server.getUrl() + "', {reconnect = {jitter = 3}})").find("jitter between 0 and 1"), std::string::npos);
}

} // namespace haylen::net
