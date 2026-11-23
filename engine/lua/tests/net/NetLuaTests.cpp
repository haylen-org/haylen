#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/net/WebSocket.hpp"
#include "haylen/platform/DevelopmentSession.hpp"
#include "net/WebSocketFixture.hpp"
#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::net {

class NetLuaTest : public WebSocketFixture {};

TEST_F(NetLuaTest, TalksThroughWebSocketsFromLua) {
    const EchoServer server;
    test::LuaFixture fixture;
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

    for (const std::string protocol : {"chat\\r\\nX-Injected: 1", "chat json", "", "chat/2"}) {
        EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "', {protocols = {'" + protocol + "'}})").find("is not a token"), std::string::npos) << protocol;
    }

    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "', {protocols = {'chat', 'json', 'chat'}})").find("The WebSocket subprotocol \"chat\" is listed twice."), std::string::npos);
    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "'):close(4294968296)").find("integer out of range"), std::string::npos);
    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "', {reconnect = {initialDelay = 0 / 0}})").find("finite delays"), std::string::npos);
}

TEST_F(NetLuaTest, ModuleReloadsEndTheListenersOfTheirTopLevel) {
    const EchoServer server;
    const std::string listener = "socket:on('message', function() heard = heard + 1 end) return {version = 1}";
    test::LuaFixture fixture({{"source/main.lua", "heard = 0 socket = require('haylen.net').connectWebSocket('" + server.getUrl() + "') require('listener')"}, {"source/listener.lua", listener}}, nullptr, {.development = true});
    fixture.frames(1);

    fixture.package().setFile("source/listener.lua", test::TestFiles::bytes("-- Edited.\n" + listener));
    const std::vector<std::string> paths{"source/listener.lua"};
    platform::DevelopmentSession* session = fixture.host().getDevelopmentSession();
    ASSERT_NE(session, nullptr);
    session->addChanges(paths);
    fixture.frames(1);
    ASSERT_FALSE(fixture.engine().isRestartRequested()) << "The module reloads in place.";
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return socket.state") == "open"; }));
    fixture.runLua("socket:send('ping')");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return heard") != "0"; }));
    EXPECT_EQ(fixture.lua("return heard"), "1") << "The listener of the first load ended with the reload.";
}

TEST_F(NetLuaTest, ReportsFailedConnectionsWithTheirAddress) {
    const std::string url = refusedUrl();
    test::LuaFixture fixture;
    fixture.runLua("local socket = require('haylen.net').connectWebSocket('" + url + "') socket:on('error', function(message) failure = socket.url .. ' ' .. tostring(#message > 0) end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(failure)") != "nil"; }));
    EXPECT_EQ(fixture.lua("return failure"), url + " true");
}

TEST_F(NetLuaTest, FailuresEndWithWhatTheProjectLacksForTheNetwork) {
    const std::string url = refusedUrl();
    const std::string requirement = "The app does not declare the permission \"android.permission.INTERNET\".";
    test::LuaFixture fixture;
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
    test::LuaFixture fixture;
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
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        net = require('haylen.net')
        local holder = {}
        owned = net.connectWebSocket(')" + server.getUrl() + R"(', {owner = holder, connectTimeout = 5})
        alsoOwned = net.connectWebSocket(')" + server.getUrl() + R"(', {owner = holder})
        kept = net.connectWebSocket(')" + server.getUrl() + R"(')
        release = function() holder = nil collectgarbage() collectgarbage() end
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return owned.state .. alsoOwned.state .. kept.state") == "openopenopen"; }));
    EXPECT_EQ(fixture.lua("return owned.bufferedAmount"), "0");
    fixture.runLua("release()");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return owned.state .. alsoOwned.state") == "closedclosed"; }));
    EXPECT_EQ(fixture.lua("return kept.state"), "open");
    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "', {connectTimeout = 0})").find("A WebSocket needs a connect timeout above 0 and at most 3600 seconds."), std::string::npos);
    EXPECT_NE(fixture.lua("net.connectWebSocket('" + server.getUrl() + "', {owner = 5})").find("An owner must be a table or a userdata, not number."), std::string::npos);
}

TEST_F(NetLuaTest, KeepsListenersAwayFromEndedAndOversizedConnections) {
    const EchoServer server;
    test::LuaFixture fixture;
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

} // namespace haylen::net
