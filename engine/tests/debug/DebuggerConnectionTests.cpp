#include <gtest/gtest.h>

#include <Poco/Buffer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/WebSocket.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/lua/Runtime.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::debug {

// A development server whose client of the debugger sets a breakpoint as soon as an app says hello, and at every halt sends a batch of files, reads the stack and lets the app go on, the way an editor does through the server of `haylen.py`.
class DebuggingServer final {
  public:
    DebuggingServer() : listener(Poco::Net::SocketAddress("127.0.0.1", 0)), server(new Factory(*this), listener, new Poco::Net::HTTPServerParams()) {
        server.start();
    }
    ~DebuggingServer() {
        server.stopAll(true);
    }

    DebuggingServer(const DebuggingServer&) = delete;
    DebuggingServer& operator=(const DebuggingServer&) = delete;

    [[nodiscard]] std::string getUrl() const {
        return "ws://127.0.0.1:" + std::to_string(listener.address().port()) + "/haylen/development?token=test";
    }

    // The messages of the debugger that reached the server, of the type `response` or `event`, with the command or the event of the name.
    [[nodiscard]] std::vector<core::Json> find(const std::string& type, const std::string& name) {
        const std::scoped_lock lock(mutex);
        std::vector<core::Json> found;
        for (const core::Json& message : debugMessages) {
            if (message.value("type", "") == type && message.value(type == "event" ? "event" : "command", "") == name) {
                found.push_back(message);
            }
        }
        return found;
    }

    // Whether the app reported that it reloaded the module of the path in place.
    [[nodiscard]] bool hasReloaded(const std::string& path) {
        const std::scoped_lock lock(mutex);
        // clang-format off
        return std::ranges::any_of(reports, [&path](const core::Json& report) {
            const core::Json modules = report.value("modules", core::Json::array());
            return std::find(modules.begin(), modules.end(), core::Json(path)) != modules.end();
        });
        // clang-format on
    }

    // Sends a batch with one file to the app that is connected, as a save on the machine of the developer does.
    void push(const std::string& path, const std::string& content) {
        const std::scoped_lock lock(mutex);
        if (connected != nullptr) {
            sendBatch(*connected, path, content);
        }
    }

  private:
    class Handler final : public Poco::Net::HTTPRequestHandler {
      public:
        explicit Handler(DebuggingServer& owner) : server(owner) {}

        void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
            Poco::Net::WebSocket socket(request, response);
            socket.setReceiveTimeout(Poco::Timespan(10, 0));
            Poco::Buffer<char> frame(0);
            try {
                while (true) {
                    int flags = 0;
                    frame.resize(0);
                    const int length = socket.receiveFrame(frame, flags);
                    if (length <= 0 || (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) == Poco::Net::WebSocket::FRAME_OP_CLOSE) {
                        break;
                    }
                    server.receive(socket, core::Json::parse(std::string_view(frame.begin(), frame.size())));
                }
            } catch (const Poco::Exception&) {}
            server.leave(socket);
        }

      private:
        DebuggingServer& server;
    };

    class Factory final : public Poco::Net::HTTPRequestHandlerFactory {
      public:
        explicit Factory(DebuggingServer& owner) : server(owner) {}

        Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override {
            return new Handler(server);
        }

      private:
        DebuggingServer& server;
    };

    static void sendText(Poco::Net::WebSocket& socket, const core::Json& message) {
        const std::string text = message.dump();
        socket.sendFrame(text.data(), static_cast<int>(text.size()), Poco::Net::WebSocket::FRAME_TEXT);
    }

    void sendRequest(Poco::Net::WebSocket& socket, const std::string& command, core::Json arguments = core::Json::object()) {
        sendText(socket, {{"type", "debug"}, {"message", {{"seq", ++sequence}, {"type", "request"}, {"command", command}, {"arguments", std::move(arguments)}}}});
    }

    void sendBatch(Poco::Net::WebSocket& socket, const std::string& path, const std::string& content) {
        sendText(socket, {{"type", "files"}, {"session", "run"}, {"revision", ++revision}, {"files", {{{"path", path}, {"size", content.size()}}}}, {"removed", core::Json::array()}});
        socket.sendFrame(content.data(), static_cast<int>(content.size()), Poco::Net::WebSocket::FRAME_BINARY);
    }

    void receive(Poco::Net::WebSocket& socket, const core::Json& message) {
        const std::scoped_lock lock(mutex);
        const std::string type = message.value("type", "");
        if (type == "hello") {
            connected = &socket;
            sendRequest(socket, "initialize", {{"adapterID", "haylen"}});
            sendRequest(socket, "attach");
            sendRequest(socket, "setBreakpoints", {{"source", {{"path", "source/mod.lua"}}}, {"breakpoints", {{{"line", 3}}}}});
            return;
        }
        if (type == "report") {
            reports.push_back(message);
            return;
        }
        if (type != "debug") {
            return;
        }
        const core::Json& debug = message.at("message");
        debugMessages.push_back(debug);
        if (debug.value("event", "") == "stopped") {
            sendBatch(socket, "content/data.json", R"({"level": 9})");
            sendRequest(socket, "stackTrace", {{"threadId", 1}});
        } else if (debug.value("command", "") == "stackTrace") {
            sendRequest(socket, "continue", {{"threadId", 1}});
        }
    }

    void leave(const Poco::Net::WebSocket& socket) {
        const std::scoped_lock lock(mutex);
        if (connected == &socket) {
            connected = nullptr;
        }
    }

    Poco::Net::ServerSocket listener;
    Poco::Net::HTTPServer server;
    std::mutex mutex;
    std::vector<core::Json> debugMessages;
    std::vector<core::Json> reports;
    Poco::Net::WebSocket* connected = nullptr;
    int sequence = 0;
    int revision = 0;
};

// The client of the debugger reaches the app through the development connection, which still carries its requests while the app halts and holds everything else for after the halt, and the breakpoints of the development session outlive restarts and reloads of the app.
TEST(DebuggerConnectionTest, HaltsThroughTheDevelopmentServerAcrossRestartsAndReloads) {
    DebuggingServer server;
    test::EngineFixture fixture({{"source/mod.lua", "local M = {}\nfunction M.run()\n    local value = 7\n    return value\nend\nreturn M\n"}, {"content/data.json", R"({"level": 1})"}}, nullptr, {.developmentServer = server.getUrl()});
    ASSERT_TRUE(fixture.frameUntil([&] { return !server.find("response", "setBreakpoints").empty(); }));
    // clang-format off
    const auto run = [&fixture] {
        try {
            lua::Runtime::runChunk(fixture.lua(), "result = require('mod').run()", "=test");
        } catch (const lua::Error& error) {
            ADD_FAILURE() << error.what();
        }
    };
    // clang-format on

    run();
    ASSERT_EQ(server.find("event", "stopped").size(), 1U);
    const core::Json frame = server.find("response", "stackTrace").front().at("body").at("stackFrames").at(0);
    EXPECT_EQ(frame.at("source").at("path"), "source/mod.lua");
    EXPECT_EQ(frame.at("line"), 3);
    EXPECT_EQ(fixture.lua("return result"), "7");
    EXPECT_EQ(fixture.lua("return require('haylen.assets').json('data.json').level"), "1");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return require('haylen.assets').json('data.json').level") == "9"; }));

    fixture.restart();
    run();
    EXPECT_EQ(server.find("event", "stopped").size(), 2U);

    server.push("source/mod.lua", "local M = {}\nfunction M.run()\n    local value = 8\n    return value\nend\nreturn M\n");
    ASSERT_TRUE(fixture.frameUntil([&] { return server.hasReloaded("source/mod.lua"); }));
    run();
    EXPECT_EQ(server.find("event", "stopped").size(), 3U);
    EXPECT_EQ(fixture.lua("return result"), "8");
}

} // namespace haylen::debug
