#include <gtest/gtest.h>

#include <string>

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
        platform.register('math.double', function(params) return {value = params.value * 2} end)
        platform.register('math.fail', function() error('handler failed') end)
        async.spawn(function()
            local info = platform.call('engine.info'):await()
            local doubled = platform.call('math.double', {value = 21}):await()
            local _, failure = platform.call('math.fail'):await()
            local device = platform.call('device.info'):await()
            local _, denied = platform.call('auth.login', {provider = 'google'}):await()
            summary = info.platform .. ' ' .. doubled.value .. ' ' .. tostring(failure:find('handler failed') ~= nil) .. ' ' .. device.model .. ' ' .. denied
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
    EXPECT_NE(fixture.lua("platform.register('x', 'not a function')").find("error: "), std::string::npos);
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
        slow, slowId = platform.call('native.slow', {n = 1})
        denied, deniedId = platform.call('native.denied')
        async.spawn(function()
            local value = slow:await()
            local _, failure = denied:await()
            summary = tostring(value.done) .. ' ' .. failure
        end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return platform.pendingCalls() .. ' ' .. tostring(math.type(slowId)) .. ' ' .. tostring(slowId ~= deniedId)"), "2 integer true");
    EXPECT_EQ(fixture.host().getPlatformCalls().back().id, std::stoull(fixture.lua("return deniedId")));

    fixture.runLua("platform.resolve(slowId, true, {done = true}) platform.resolve(deniedId, false, {message = 'no network'})");
    fixture.runLua("platform.emit('app.link', {url = 'island://beach'})");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return summary ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return summary .. ' ' .. platform.pendingCalls() .. ' ' .. table.concat(links, ',')"), "true no network 0 island://beach");
    EXPECT_NE(fixture.lua("platform.emit('app.link', print)").find("cannot be converted to JSON"), std::string::npos);
    EXPECT_NE(fixture.lua("platform.resolve(-1, true)").find("expected a non-negative integer"), std::string::npos);
}

TEST(PlatformLuaTest, CppCallsToLuaHandlersReturnErrorsInsteadOfCrashing) {
    test::EngineFixture fixture;
    fixture.runLua("require('haylen.platform').register('bad.result', function() return {callback = print} end)");

    Bridge::Result received;
    fixture.engine().getPlatform().call("bad.result", core::Json::object(), [&](Bridge::Result result) { received = std::move(result); });
    fixture.frames(1);
    EXPECT_FALSE(received.ok);
    EXPECT_NE(received.error.find("cannot be converted to JSON"), std::string::npos);
}

} // namespace haylen::platform
