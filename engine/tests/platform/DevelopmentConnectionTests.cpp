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
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/core/Log.hpp"
#include "platform/DevelopmentSession.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "platform/headless/HeadlessPlayer.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

// A development server that pushes one batch to every app that says hello, with a module, an asset and a path outside the package, restarts the app once it reports the batch, as the developer asks with R, and keeps the messages the app sends.
class PushingServer final {
  public:
    PushingServer() : listener(Poco::Net::SocketAddress("127.0.0.1", 0)), server(new Factory(*this), listener, new Poco::Net::HTTPServerParams()) {
        server.start();
    }
    ~PushingServer() {
        server.stopAll(true);
    }

    PushingServer(const PushingServer&) = delete;
    PushingServer& operator=(const PushingServer&) = delete;

    [[nodiscard]] std::string getUrl() const {
        return "ws://127.0.0.1:" + std::to_string(listener.address().port()) + "/haylen/development?token=test";
    }

    [[nodiscard]] std::vector<core::Json> getMessages() {
        const std::scoped_lock lock(mutex);
        return messages;
    }

  private:
    class Handler final : public Poco::Net::HTTPRequestHandler {
      public:
        explicit Handler(PushingServer& owner) : server(owner) {}

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
                        return;
                    }
                    const core::Json message = core::Json::parse(std::string_view(frame.begin(), frame.size()));
                    server.record(message);
                    if (message.at("type") == "hello") {
                        push(socket);
                    } else if (message.at("type") == "report" && !message.value("restarted", false)) {
                        const std::string restart = R"({"type": "restart"})";
                        socket.sendFrame(restart.data(), static_cast<int>(restart.size()), Poco::Net::WebSocket::FRAME_TEXT);
                    }
                }
            } catch (const Poco::Exception&) {}
        }

      private:
        // Messages of the wrong shape come first, and the app drops them without stopping.
        static void push(Poco::Net::WebSocket& socket) {
            for (const std::string malformed : {R"({"type": 1})", R"({"type": "files", "files": [1]})", R"({"type": "files", "files": [], "removed": [2]})", R"({"type": "files", "files": [], "revision": "one"})", R"({"type": "shaderError", "path": 3})"}) {
                socket.sendFrame(malformed.data(), static_cast<int>(malformed.size()), Poco::Net::WebSocket::FRAME_TEXT);
            }
            const std::vector<std::pair<std::string, std::string>> files{{"source/counter.lua", "return {value = 'pushed'}"}, {"content/data.json", R"({"level": 9})"}, {"../secret.lua", "return 'stolen'"}};
            core::Json listed = core::Json::array();
            for (const auto& [path, content] : files) {
                listed.push_back({{"path", path}, {"size", content.size()}});
            }
            const std::string message = core::Json{{"type", "files"}, {"session", "run"}, {"revision", 1}, {"files", listed}, {"removed", core::Json::array()}}.dump();
            socket.sendFrame(message.data(), static_cast<int>(message.size()), Poco::Net::WebSocket::FRAME_TEXT);
            for (const auto& [path, content] : files) {
                socket.sendFrame(content.data(), static_cast<int>(content.size()), Poco::Net::WebSocket::FRAME_BINARY);
            }
        }

        PushingServer& server;
    };

    class Factory final : public Poco::Net::HTTPRequestHandlerFactory {
      public:
        explicit Factory(PushingServer& owner) : server(owner) {}

        Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override {
            return new Handler(server);
        }

      private:
        PushingServer& server;
    };

    void record(core::Json message) {
        const std::scoped_lock lock(mutex);
        messages.push_back(std::move(message));
    }

    Poco::Net::ServerSocket listener;
    Poco::Net::HTTPServer server;
    std::mutex mutex;
    std::vector<core::Json> messages;
};

TEST(DevelopmentConnectionTest, AppliesPushedFilesAndRefusesPathsOutsideThePackage) {
    PushingServer server;
    std::vector<std::string> lines;
    std::mutex linesMutex;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&](core::Log::Level, std::string_view line) {
        const std::scoped_lock lock(linesMutex);
        lines.emplace_back(line);
    });
    // clang-format on

    test::EngineFixture fixture({{"source/counter.lua", "return {value = 'packaged'}"}, {"content/data.json", R"({"level": 1})"}, {"source/main.lua", "counter = require('counter')"}}, nullptr, {.developmentServer = server.getUrl()});
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return counter.value") == "pushed"; }));
    EXPECT_EQ(fixture.lua("return require('haylen.assets').json('data.json').level"), "9");
    EXPECT_FALSE(fixture.engine().isRestartRequested());

    // The report of the batch goes back to the server with the revision it answers.
    ASSERT_TRUE(fixture.frameUntil([&] { return std::ranges::any_of(server.getMessages(), [](const core::Json& message) { return message.at("type") == "report"; }); }));
    const std::vector<core::Json> messages = server.getMessages();
    EXPECT_EQ(messages.front().at("type"), "hello");
    EXPECT_EQ(messages.front().at("session"), "");
    const auto report = std::ranges::find_if(messages, [](const core::Json& message) { return message.at("type") == "report"; });
    EXPECT_EQ(report->at("revision"), 1);
    EXPECT_EQ(report->at("modules"), core::Json::array({"source/counter.lua"}));

    // The restart that the server asks for restarts the app, which tells the server why.
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().isRestartRequested(); }));
    ASSERT_TRUE(fixture.frameUntil([&] { return std::ranges::any_of(server.getMessages(), [](const core::Json& message) { return message.at("type") == "report" && message.value("restarted", false); }); }));
    core::Log::removeListener(listener);
    const std::scoped_lock lock(linesMutex);
    EXPECT_TRUE(std::ranges::any_of(lines, [](const std::string& line) { return line.find("The development server sent the path \"../secret.lua\", which is not a file of the package, so it was refused.") != std::string::npos; }));
    EXPECT_EQ(std::ranges::count_if(lines, [](const std::string& line) { return line.find("The development server sent a message that this engine does not know.") != std::string::npos; }), 4);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

// An app that launches another app with the address of a development server, as a runner of apps does, plays that app in development with the files the server sends, and starts again as it ships once that app quits.
TEST(DevelopmentConnectionTest, PlaysALaunchedAppInDevelopmentWithTheServerItNames) {
    PushingServer server;
    test::TemporaryDirectory guest;
    guest.write("app.json", R"({"name": "Guest", "identifier": "com.haylen.tests"})");
    guest.write("source/counter.lua", "return {value = 'packaged'}");
    // clang-format off
    guest.write("source/main.lua", R"(
        local haylen = require('haylen')
        local preferences = require('haylen.preferences')
        local counter = require('counter')
        require('haylen.scene').push({update = function()
            if counter.value == 'pushed' then
                preferences.set('pushed', true)
                preferences.save()
                haylen.quit()
            end
        end})
    )");
    test::TemporaryDirectory launcher;
    launcher.write("app.json", R"({"name": "Launcher", "identifier": "com.haylen.tests"})");
    launcher.write("source/main.lua", R"(
        local haylen = require('haylen')
        local preferences = require('haylen.preferences')
        if preferences.get('pushed', false) then
            preferences.remove('pushed')
            preferences.save()
            haylen.quit({status = haylen.launched() and 1 or 7})
        else
            haylen.launch(')" + guest.getPath().generic_string() + "', {developmentServer = '" + server.getUrl() + R"('})
        end
    )");
    // clang-format on

    EXPECT_EQ(HeadlessPlayer::run({.package = launcher.getPath(), .frames = 600}), 7);
    const std::vector<core::Json> messages = server.getMessages();
    EXPECT_TRUE(std::ranges::any_of(messages, [](const core::Json& message) { return message.at("type") == "hello"; }));
}

// A server outside the device is on the local network, which a platform such as Android 17 lets the app reach only once the person grants it, so the session asks first and says why it never connects when the person refuses, instead of waiting for packets the platform drops.
TEST(DevelopmentConnectionTest, AsksForTheLocalNetworkBeforeItReachesAServerOutsideTheDevice) {
    std::vector<std::string> lines;
    std::mutex linesMutex;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&](core::Log::Level, std::string_view line) {
        const std::scoped_lock lock(linesMutex);
        lines.emplace_back(line);
    });
    // clang-format on

    const test::TemporaryDirectory directory;
    HeadlessHost host(directory.getPath());
    host.setPermissionAnswer({.state = Permission::State::Denied});
    DevelopmentSession refused(host, std::nullopt, "ws://192.0.2.10:7777/haylen/development");
    refused.pump();
    refused.pump();
    EXPECT_EQ(host.getPermissionRequests(), (std::vector<Permission::Kind>{Permission::Kind::LocalNetwork}));

    // A server on the device, such as the one that "adb reverse" forwards, never asks.
    DevelopmentSession onDevice(host, std::nullopt, "ws://127.0.0.1:7777/haylen/development");
    EXPECT_EQ(host.getPermissionRequests().size(), 1U);

    core::Log::removeListener(listener);
    const std::scoped_lock lock(linesMutex);
    EXPECT_EQ(std::ranges::count_if(lines, [](const std::string& line) { return line.find("The app does not connect to the development server at \"ws://192.0.2.10:7777/haylen/development\". The person did not grant the access to the local network that reaching it needs.") != std::string::npos; }), 1);
    EXPECT_FALSE(std::ranges::any_of(lines, [](const std::string& line) { return line.find("out of reach") != std::string::npos; }));
}

} // namespace haylen::platform
