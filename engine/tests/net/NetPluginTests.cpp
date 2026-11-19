#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/net/WebSocket.hpp"
#include "haylen/plugins/NetPlugin.hpp"
#include "net/WebSocketFixture.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::net {

class NetPluginTest : public WebSocketFixture {};

TEST_F(NetPluginTest, PumpsSocketsWithoutAllocating) {
    const EchoServer server;
    test::EngineFixture fixture;
    plugins::NetPlugin& plugin = fixture.engine().getPlugin<plugins::NetPlugin>();
    const std::shared_ptr<WebSocket> socket = plugin.connectWebSocket(server.getUrl());
    ASSERT_TRUE(fixture.frameUntil([&] { return socket->getState() == WebSocket::State::Open; }));
    plugin.beginFrame(fixture.engine(), 0.0F);

    const test::AllocationTracker tracker;
    plugin.beginFrame(fixture.engine(), 0.0F);
    EXPECT_EQ(tracker.getTotal(), 0U);
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
