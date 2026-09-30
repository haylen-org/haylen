#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/platform/Bridge.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::platform {

TEST(PlatformLuaTest, CallsNativeAndLuaHandlersWithPromises) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        platform = require('haylen.platform')
        async = require('async')
        platform.registerHandler('math.double', function(params) return {value = params.value * 2} end)
        platform.registerHandler('math.fail', function() error('handler failed') end)
        async.spawn(function()
            local doubled = platform.call('math.double', {value = 21}):await()
            local _, failure = platform.call('math.fail'):await()
            local device = platform.call('device.model'):await()
            local _, denied = platform.call('auth.login', {provider = 'google'}):await()
            summary = doubled.value .. ' ' .. tostring(failure.message:find('handler failed') ~= nil) .. ' ' .. device.model .. ' ' .. denied
        end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return platform.hasHandler('math.double') and not platform.hasHandler('device.model')"), "true");

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.host().getPlatformCalls().size() == 1; }));
    fixture.engine().getPlatform().resolve(fixture.host().getPlatformCalls()[0].id, true, R"({"model": "phone"})");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.host().getPlatformCalls().size() == 2; }));
    EXPECT_EQ(fixture.host().getPlatformCalls()[1].paramsJson, R"({"provider":"google"})");
    fixture.engine().getPlatform().resolve(fixture.host().getPlatformCalls()[1].id, false, R"("denied")");

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return summary ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return summary"), "42 true phone denied");
    EXPECT_NE(fixture.lua("platform.registerHandler('x', 'not a function')").find("error: "), std::string::npos);
}

TEST(PlatformLuaTest, SubscribesToNativeEvents) {
    test::EngineFixture fixture;
    fixture.runLua("platform = require('haylen.platform') links = {} connection = platform.on('app.link', function(payload) links[#links + 1] = payload.url end)");
    EXPECT_EQ(fixture.lua("return connection.connected"), "true");

    fixture.engine().getPlatform().emit("app.link", R"({"url": "island://a"})");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(links, ',')"), "island://a");

    fixture.runLua("connection:disconnect()");
    EXPECT_EQ(fixture.lua("return connection.connected"), "false");
    fixture.engine().getPlatform().emit("app.link", R"({"url": "island://b"})");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return #links"), "1");

    fixture.runLua("platform.on('app.crash', function() error('listener failed') end)");
    fixture.engine().getPlatform().emit("app.crash", "{}");
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("listener failed"), std::string::npos);
}

TEST(PlatformLuaTest, AnswersCallsAndSendsEventsFromLua) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        platform = require('haylen.platform')
        async = require('async')
        links = {}
        platform.on('app.link', function(payload) links[#links + 1] = payload.url end)
        slow = platform.call('native.slow', {n = 1})
        denied = platform.call('native.denied')
        slowId, deniedId = slow.id, denied.id
        async.spawn(function()
            local value = slow:await()
            local _, failure = denied:await()
            summary = tostring(value.done) .. ' ' .. failure
        end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return platform.pendingCallCount() .. ' ' .. tostring(math.type(slowId)) .. ' ' .. tostring(slowId ~= deniedId)"), "2 integer true");
    EXPECT_EQ(fixture.host().getPlatformCalls().back().id, std::stoull(fixture.lua("return deniedId")));

    fixture.runLua("platform.resolve(slowId, true, {done = true}) platform.resolve(deniedId, false, {message = 'no network'})");
    fixture.runLua("platform.emit('app.link', {url = 'island://beach'})");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return summary ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return summary .. ' ' .. platform.pendingCallCount() .. ' ' .. table.concat(links, ',')"), "true no network 0 island://beach");
    EXPECT_NE(fixture.lua("platform.emit('app.link', print)").find("cannot be converted to JSON"), std::string::npos);
    EXPECT_NE(fixture.lua("platform.resolve(-1, true)").find("expected a non-negative integer"), std::string::npos);
}

TEST(PlatformLuaTest, FailsCallsWithTypedErrorsTimeoutsAndCancellation) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        platform = require('haylen.platform')
        async = require('async')
        platform.registerHandler('app.name', function() return 'Island' end)
        platform.registerHandler('app.build', function() return 7 end)
        slow = platform.call('store.slow', {item = 1}, {timeout = 0.05})
        typed = platform.call('store.typed')
        given = platform.call('store.given')
        async.spawn(function()
            local _, timeout = slow:await()
            local _, typedError = typed:await()
            local _, cancel = given:await()
            local all = async.all({platform.call('app.name').promise, platform.call('app.build').promise}):await()
            summary = table.concat({timeout.code, timeout.message, typedError.code, typedError.message, typedError.data.retry, cancel.code, tostring(given.done), all[1], all[2]}, '|')
        end)
    )");
    // clang-format on
    ASSERT_EQ(fixture.host().getPlatformCalls().size(), 3U);
    fixture.engine().getPlatform().resolve(std::stoull(fixture.lua("return typed.id")), false, R"({"message": "The store is closed.", "code": "closed", "data": {"retry": 30}})");
    EXPECT_EQ(fixture.lua("return given:cancel()"), "true");

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return summary ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return summary"), "timeout|The platform call store.slow timed out.|closed|The store is closed.|30|cancelled|true|Island|7");
    EXPECT_EQ(fixture.host().getCancelledCalls(), (std::vector<std::uint64_t>{std::stoull(fixture.lua("return given.id")), std::stoull(fixture.lua("return slow.id"))}));
    EXPECT_EQ(fixture.lua("return given:cancel()"), "false");

    EXPECT_NE(fixture.lua("platform.call('store.slow', nil, {timeout = 0})").find("positive number of seconds"), std::string::npos);
    EXPECT_NE(fixture.lua("platform.call('store.slow', nil, {timeout = 'soon'})").find("timeout"), std::string::npos);
    EXPECT_NE(fixture.lua("platform.call('store.slow', nil, {deadline = 1})").find("deadline"), std::string::npos);
}

TEST(PlatformLuaTest, SendsCallsAndRetainsEventsFromLua) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        platform = require('haylen.platform')
        tracked = {}
        platform.registerHandler('analytics.track', function(params) tracked[#tracked + 1] = params.name return true end)
        platform.send('analytics.track', {name = 'start'})
        platform.send('store.restore')
        platform.emit('app.opened', {url = 'island://a'}, {retain = true})
        platform.emit('app.opened', {url = 'island://b'}, {retain = false})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return table.concat(tracked, ',') .. ' ' .. platform.pendingCallCount()"), "start 0");
    ASSERT_EQ(fixture.host().getPlatformCalls().size(), 1U);
    EXPECT_EQ(fixture.host().getPlatformCalls().front().method, "store.restore");
    EXPECT_EQ(fixture.host().getPlatformCalls().front().paramsJson, "{}");
    fixture.engine().getPlatform().resolve(fixture.host().getPlatformCalls().front().id, true, "true");
    fixture.frames(1);

    // The retained event waited for the listener that connected after it, and the plain one was dropped.
    fixture.runLua("opened = {} platform.on('app.opened', function(payload) opened[#opened + 1] = payload.url end)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(opened, ',') .. ' ' .. platform.pendingCallCount()"), "island://a 0");
    EXPECT_NE(fixture.lua("platform.emit('app.opened', {}, {keep = true})").find("keep"), std::string::npos);
    EXPECT_NE(fixture.lua("platform.emit('app.opened', {}, true)").find("table expected"), std::string::npos);
    EXPECT_NE(fixture.lua("platform.send('')").find("A platform call needs a method name."), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(PlatformLuaTest, HandsPluginModulesTheHandlesOfTheirPlugins) {
    // clang-format off
    test::EngineFixture fixture({
        {"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "plugins": {"ads-kit": {"testMode": true}, "sign-in": {}}})"},
        {"plugins/ads-kit/plugin.json", R"({"id": "ads-kit", "version": "1.2.0", "parameters": {"testMode": {"type": "boolean", "default": false}, "placement": {"type": "string", "default": "bottom"}, "appId": {"type": "string"}}})"},
        {"plugins/sign-in/plugin.json", R"({"id": "sign-in", "version": "2.0"})"},
        {"plugins/ads-kit/source/init.lua", "local handle = require('haylen.platform').plugin('ads-kit') return {handle = handle}"},
    });
    // clang-format on
    fixture.host().setNativePlugins({"ads-kit"});
    // clang-format off
    fixture.runLua(R"(
        platform = require('haylen.platform')
        ads = require('ads-kit').handle
        closed = {}
        connection = ads:on('closed', function(payload) closed[#closed + 1] = payload.reason end)
        shown = ads:call('show', {format = 'banner'}, {timeout = 30})
        ads:send('track', {name = 'opened'})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return table.concat({ads.id, ads.version, tostring(ads.native), getmetatable(ads)}, ' ')"), "ads-kit 1.2.0 true haylen.AppPlugin");
    EXPECT_EQ(fixture.lua("return table.concat({tostring(ads.config.testMode), ads.config.placement, tostring(ads.config.appId)}, ' ')"), "true bottom nil");
    EXPECT_EQ(fixture.lua("local all = platform.plugins() return table.concat({#all, all[1].id, all[1].version, tostring(all[1].native), all[2].id, all[2].version, tostring(all[2].native)}, ' ')"), "2 ads-kit 1.2.0 true sign-in 2.0 false");
    EXPECT_EQ(fixture.lua("return tostring(platform.plugin('sign-in').native)"), "false");

    // Calls, sends and events of the handle carry the id of the plugin in front of their names, and a send creates no call.
    const std::vector<HeadlessHost::PlatformCall>& calls = fixture.host().getPlatformCalls();
    ASSERT_EQ(calls.size(), 2U);
    EXPECT_EQ(calls[0].method, "ads-kit.show");
    EXPECT_EQ(calls[0].paramsJson, R"({"format":"banner"})");
    EXPECT_EQ(calls[1].method, "ads-kit.track");
    EXPECT_EQ(fixture.lua("return platform.pendingCallCount()"), "1");
    fixture.engine().getPlatform().resolve(calls[1].id, false, R"("dropped")");
    fixture.engine().getPlatform().resolve(calls[0].id, true, R"({"shown": true})");
    fixture.engine().getPlatform().emit("ads-kit.closed", R"({"reason": "dismissed"})");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat({tostring(shown.done), table.concat(closed, ','), platform.pendingCallCount()}, ' ')"), "true dismissed 0");
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    EXPECT_NE(fixture.lua("platform.plugin('crash-kit')").find("The plugin crash-kit is not among the plugins of app.json."), std::string::npos);
    EXPECT_NE(fixture.lua("ads:call('')").find("A method or event of the plugin ads-kit needs a name."), std::string::npos);
    EXPECT_NE(fixture.lua("ads:on('closed', 3)").find("function expected"), std::string::npos);
    EXPECT_NE(fixture.lua("ads.id = 'other'").find("error: "), std::string::npos);
}

TEST(PlatformLuaTest, ExplainsPluginManifestsItCannotRead) {
    // clang-format off
    test::EngineFixture fixture({
        {"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "plugins": {"broken": {}, "unversioned": {}}})"},
        {"plugins/broken/plugin.json", "{"},
        {"plugins/unversioned/plugin.json", R"({"id": "unversioned"})"},
    });
    // clang-format on
    fixture.runLua("platform = require('haylen.platform')");
    EXPECT_NE(fixture.lua("platform.plugin('broken')").find("The plugins/broken/plugin.json of the package is not a JSON object."), std::string::npos);
    EXPECT_NE(fixture.lua("platform.plugins()").find("The plugins/broken/plugin.json of the package is not a JSON object."), std::string::npos);
    fixture.package().setFile("plugins/broken/plugin.json", test::TestFiles::bytes(R"({"id": "broken", "version": "1"})"));
    EXPECT_NE(fixture.lua("platform.plugin('unversioned')").find("The plugins/unversioned/plugin.json of the package has no version."), std::string::npos);
}

TEST(PlatformLuaTest, CppCallsToLuaHandlersReturnErrorsInsteadOfCrashing) {
    test::EngineFixture fixture;
    fixture.runLua("require('haylen.platform').registerHandler('bad.result', function() return {callback = print} end)");

    Bridge::Result received;
    fixture.engine().getPlatform().call("bad.result", core::Json::object(), [&](Bridge::Result result) { received = std::move(result); });
    fixture.frames(1);
    EXPECT_FALSE(received.ok);
    EXPECT_NE(received.error.message.find("cannot be converted to JSON"), std::string::npos);
}

} // namespace haylen::platform
