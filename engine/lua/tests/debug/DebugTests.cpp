#include <gtest/gtest.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Log.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::debug {

TEST(DebugPluginTest, CyclesTheStatisticsAndKeepsRecentLines) {
    test::LuaFixture fixture;
    haylen::plugins::DebugPlugin& plugin = fixture.engine().getPlugin<haylen::plugins::DebugPlugin>();
    haylen::core::Log::warning("wolves nearby");
    std::vector<haylen::debug::LogLine> recent;
    plugin.readRecentLog(recent);
    ASSERT_FALSE(recent.empty());
    EXPECT_EQ(recent.back().level, haylen::core::Log::Level::Warning);
    EXPECT_NE(recent.back().text.find("wolves nearby"), std::string::npos);

    // clang-format off
    const auto press = [&](haylen::input::Key key) {
        haylen::platform::Event event;
        event.type = haylen::platform::Event::Type::KeyDown;
        event.key = key;
        fixture.engine().handleEvent(event);
    };
    // clang-format on
    EXPECT_EQ(plugin.getStatsMode(), haylen::debug::StatsDisplay::Mode::Off);
    press(haylen::input::Key::F3);
    EXPECT_EQ(plugin.getStatsMode(), haylen::debug::StatsDisplay::Mode::Compact);
    fixture.frames(3);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().instances, 0U);
    press(haylen::input::Key::F3);
    EXPECT_EQ(plugin.getStatsMode(), haylen::debug::StatsDisplay::Mode::Full);
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().vertices, 0U);

    plugin.setToggleKey(haylen::input::Key::F4);
    press(haylen::input::Key::F3);
    EXPECT_EQ(plugin.getStatsMode(), haylen::debug::StatsDisplay::Mode::Full);
    press(haylen::input::Key::F4);
    EXPECT_EQ(plugin.getStatsMode(), haylen::debug::StatsDisplay::Mode::Off);
    plugin.setToggleKey(std::nullopt);
    press(haylen::input::Key::F4);
    EXPECT_EQ(plugin.getStatsMode(), haylen::debug::StatsDisplay::Mode::Off);
    EXPECT_EQ(plugin.getToggleKey(), std::nullopt);

    for (std::size_t line = 0; line < haylen::plugins::DebugPlugin::kLogLines + 20; ++line) {
        haylen::core::Log::info("line {}", line);
    }

    plugin.readRecentLog(recent);
    EXPECT_EQ(recent.size(), haylen::plugins::DebugPlugin::kLogLines);
}

TEST(DebugPluginTest, ProfilesFromLua) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        doubled = 0
        require('haylen.scene').push({update = function() doubled = debugging.profile('work', function(value) return value * 2 end, 21) end})
    )");
    // clang-format on
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return doubled"), "42");
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local frame = debugging.frame()
        local found
        for _, scope in ipairs(frame.scopes) do if scope.name == 'work' then found = scope end end
        return found.depth .. ' ' .. found.calls .. ' ' .. tostring(frame.milliseconds >= 0 and frame.fps >= 0 and frame.average >= 0) .. ' ' .. tostring(#debugging.frameHistory() >= 2)
    )"), "1 1 true true");
    // clang-format on

    EXPECT_EQ(fixture.lua("debugging.setStatsMode('full') return debugging.statsMode()"), "full");
    EXPECT_EQ(fixture.lua("return debugging.toggleKey()"), "f3");
    EXPECT_EQ(fixture.lua("debugging.setToggleKey('f5') return debugging.toggleKey()"), "f5");
    EXPECT_EQ(fixture.lua("debugging.setToggleKey(nil) debugging.setStatsMode('off') return debugging.statsMode() .. ' ' .. tostring(debugging.toggleKey())"), "off nil");
    EXPECT_NE(fixture.lua("debugging.setStatsMode('verbose')").find("unknown value 'verbose'"), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.setToggleKey('nope')").find("error: "), std::string::npos);
    EXPECT_EQ(fixture.lua("require('haylen.log').warning('from lua') local lines = debugging.recentLog(1) return #lines .. ' ' .. lines[1].level .. ' ' .. tostring(lines[1].text:find('from lua') ~= nil)"), "1 warning true");
    EXPECT_NE(fixture.lua("debugging.recentLog(-1)").find("expected a non-negative integer"), std::string::npos);
    EXPECT_EQ(fixture.lua("debugging.beginScope('manual') debugging.endScope() return 'ok'"), "ok");
    EXPECT_NE(fixture.lua("debugging.endScope()").find("closed without being opened"), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.profile('bad', function() error('inside') end)").find("inside"), std::string::npos);

    // An app that closes more scopes than it opened gets the error, while the scope of the engine it runs in stays open.
    fixture.runLua("local scene = require('haylen.scene') scene.clear() scene.push({update = function() debugging.endScope() end})");
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("A profiler scope was closed without being opened."), std::string::npos);
    const std::span<const haylen::debug::ProfileSample> scopes = fixture.engine().getProfiler().getLastFrame();
    EXPECT_NE(std::ranges::find(scopes, std::string_view("update"), &haylen::debug::ProfileSample::name), scopes.end());
}

} // namespace haylen::lua::debug
