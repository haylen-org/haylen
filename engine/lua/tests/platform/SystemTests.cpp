#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "haylen/platform/SystemAnswer.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::platform {

TEST(SystemLuaTest, ReportsTheInfoThemeAndBattery) {
    test::LuaFixture fixture;
    fixture.runLua("system = require('haylen.system')");
    EXPECT_EQ(fixture.lua("local info = system.info() return info.deviceKind .. ' ' .. tostring(info.cpuCores > 0) .. ' ' .. tostring(info.gpuName) .. ' ' .. #system.languages()"), "desktop true nil 0");
    EXPECT_EQ(fixture.lua("return system.theme()"), "light");
    EXPECT_EQ(fixture.lua("local battery = system.battery() return tostring(battery.level) .. ' ' .. tostring(battery.charging) .. ' ' .. battery.state"), "nil false unknown");

    fixture.host().setSystemInfo({.os = haylen::platform::SystemInfo::Os::Android, .osVersion = "16", .deviceModel = "Phone 9", .manufacturer = "Maker", .deviceKind = haylen::platform::SystemInfo::DeviceKind::Phone, .cpuName = "Chip G4", .cpuCores = 8, .architecture = haylen::platform::SystemInfo::Architecture::Arm64, .memoryBytes = 12884901888});
    fixture.restart();
    // clang-format off
    fixture.runLua(R"(
        system = require('haylen.system')
        events = require('haylen.events')
        heard = {}
        events.on('systemThemeChanged', function(data) heard[#heard + 1] = data.theme end)
        events.on('batteryChanged', function(data) heard[#heard + 1] = data.state .. ' ' .. tostring(data.level) end)
        events.on('systemLanguagesChanged', function(data) heard[#heard + 1] = table.concat(data.languages, ',') end)
        local info = system.info()
        summary = table.concat({info.os, info.osVersion, info.deviceModel, info.manufacturer, info.deviceKind, info.cpuName, info.cpuCores, info.architecture, info.bits, info.memoryBytes}, '|')
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return summary"), "android|16|Phone 9|Maker|phone|Chip G4|8|arm64|64|12884901888");

    fixture.host().setTheme(haylen::platform::Theme::Dark);
    fixture.host().setBattery({.level = 0.5F, .charging = true, .state = haylen::platform::Battery::State::Charging});
    fixture.host().setLanguages(std::vector<std::string>{"pt-BR", "en"});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "dark, charging 0.5, pt-BR,en");
    EXPECT_EQ(fixture.lua("return table.concat(system.languages(), ',')"), "pt-BR,en");

    // The power saving mode and the time zone stay nil until the platform tells them.
    EXPECT_EQ(fixture.lua("return tostring(system.powerSaving()) .. ' ' .. tostring(system.timeZone())"), "nil nil");
    fixture.runLua("events.on('powerSavingChanged', function(data) heard[#heard + 1] = 'saving ' .. tostring(data.enabled) end) events.on('timeZoneChanged', function(data) heard[#heard + 1] = data.timeZone end)");
    fixture.host().setPowerSaving(true);
    fixture.host().setTimeZone("Asia/Tokyo");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "dark, charging 0.5, pt-BR,en, saving true, Asia/Tokyo");
    EXPECT_EQ(fixture.lua("return tostring(system.powerSaving()) .. ' ' .. system.timeZone()"), "true Asia/Tokyo");
    EXPECT_EQ(fixture.lua("local battery = system.battery() return system.theme() .. ' ' .. battery.level .. ' ' .. tostring(battery.charging) .. ' ' .. battery.state"), "dark 0.5 true charging");
}

TEST(SystemLuaTest, WritesAndReadsTheClipboardOrSaysWhyNot) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        system = require('haylen.system')
        async = require('async')
        answers = {}
        function remember(promise)
            async.spawn(function()
                local text, message, failure = promise:await()
                answers[#answers + 1] = failure and (failure.code .. ': ' .. message) or tostring(text)
            end)
        end
    )");
    // clang-format on
    fixture.runLua("remember(system.setClipboard('Island seed: 184467')) remember(system.clipboard())");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #answers") == "2"; }));
    fixture.host().setClipboardAnswer({.state = haylen::platform::SystemAnswer::State::Denied, .message = "The person has not allowed the app to read the clipboard."});
    fixture.runLua("remember(system.setClipboard('Refused')) remember(system.clipboard()) system.setClipboard('Unheard')");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #answers") == "4"; }));
    EXPECT_EQ(fixture.lua("return answers[1] .. ' | ' .. answers[2]"), "nil | Island seed: 184467");
    EXPECT_EQ(fixture.lua("return answers[4]"), "denied: The person has not allowed the app to read the clipboard.");

    // A refusal that nothing awaits never stops the app.
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_NE(fixture.lua("system.setClipboard()").find("string expected"), std::string::npos);
}

} // namespace haylen::lua::platform
