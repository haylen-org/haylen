#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/platform/Bridge.hpp"
#include "support/EngineFixture.hpp"

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
            local info = platform.call('engine.info'):await()
            local doubled = platform.call('math.double', {value = 21}):await()
            local _, failure = platform.call('math.fail'):await()
            local device = platform.call('device.info'):await()
            local _, denied = platform.call('auth.login', {provider = 'google'}):await()
            summary = info.platform .. ' ' .. doubled.value .. ' ' .. tostring(failure.message:find('handler failed') ~= nil) .. ' ' .. device.model .. ' ' .. denied
        end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return platform.hasHandler('math.double') and not platform.hasHandler('device.info')"), "true");

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.host().getPlatformCalls().size() == 1; }));
    fixture.engine().getPlatform().resolve(fixture.host().getPlatformCalls()[0].id, true, R"({"model": "phone"})");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.host().getPlatformCalls().size() == 2; }));
    EXPECT_EQ(fixture.host().getPlatformCalls()[1].paramsJson, R"({"provider":"google"})");
    fixture.engine().getPlatform().resolve(fixture.host().getPlatformCalls()[1].id, false, R"("denied")");

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return summary ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return summary"), "headless 42 true phone denied");
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
        slow = platform.call('store.slow', {item = 1}, {timeout = 0.05})
        typed = platform.call('store.typed')
        given = platform.call('store.given')
        async.spawn(function()
            local _, timeout = slow:await()
            local _, typedError = typed:await()
            local _, cancel = given:await()
            local all = async.all({platform.call('engine.info').promise, platform.call('app.version').promise}):await()
            summary = table.concat({timeout.code, timeout.message, typedError.code, typedError.message, typedError.data.retry, cancel.code, tostring(given.done), all[1].engine, all[2]}, '|')
        end)
    )");
    // clang-format on
    ASSERT_EQ(fixture.host().getPlatformCalls().size(), 3U);
    fixture.engine().getPlatform().resolve(std::stoull(fixture.lua("return typed.id")), false, R"({"message": "The store is closed.", "code": "closed", "data": {"retry": 30}})");
    EXPECT_EQ(fixture.lua("return given:cancel()"), "true");

    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return summary ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return summary"), "timeout|The platform call store.slow timed out.|closed|The store is closed.|30|cancelled|true|Haylen|1.0.0");
    EXPECT_EQ(fixture.host().getCancelledCalls(), (std::vector<std::uint64_t>{std::stoull(fixture.lua("return given.id")), std::stoull(fixture.lua("return slow.id"))}));
    EXPECT_EQ(fixture.lua("return given:cancel()"), "false");

    EXPECT_NE(fixture.lua("platform.call('store.slow', nil, {timeout = 0})").find("positive number of seconds"), std::string::npos);
    EXPECT_NE(fixture.lua("platform.call('store.slow', nil, {timeout = 'soon'})").find("timeout"), std::string::npos);
    EXPECT_NE(fixture.lua("platform.call('store.slow', nil, {deadline = 1})").find("deadline"), std::string::npos);
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
