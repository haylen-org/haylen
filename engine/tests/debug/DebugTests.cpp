#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/ProfileScope.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::debug {

TEST(ProfilerTest, AddsUpScopesPerFrame) {
    double now = 0.0;
    Profiler profiler(4, [&now] { return now; });

    profiler.beginFrame();
    now = 0.001;
    profiler.beginScope("update");
    now = 0.003;
    profiler.beginScope("physics");
    now = 0.004;
    profiler.endScope();
    {
        const ProfileScope scope(profiler, "physics");
        now = 0.006;
    }
    now = 0.007;
    profiler.endScope();
    profiler.beginScope("render");
    now = 0.010;
    profiler.endScope();
    now = 0.016;
    profiler.endFrame();

    const std::vector<ProfileSample>& samples = profiler.getLastFrame();
    ASSERT_EQ(samples.size(), 3U);
    EXPECT_EQ(samples[0].name, "update");
    EXPECT_NEAR(samples[0].milliseconds, 6.0, 1e-9);
    EXPECT_EQ(samples[1].name, "physics");
    EXPECT_NEAR(samples[1].milliseconds, 3.0, 1e-9);
    EXPECT_EQ(samples[1].calls, 2U);
    EXPECT_EQ(samples[1].depth, 1);
    EXPECT_EQ(samples[1].parent, 0);
    EXPECT_EQ(samples[2].name, "render");
    EXPECT_EQ(samples[2].depth, 0);
    EXPECT_NEAR(profiler.getLastFrameMilliseconds(), 16.0, 1e-9);
}

TEST(ProfilerTest, ScopesCloseWhatTheyContain) {
    Profiler profiler;
    profiler.beginFrame();
    {
        const ProfileScope outer(profiler, "outer");
        profiler.beginScope("left open");
    }
    EXPECT_EQ(profiler.getDepth(), 0U);

    // A scope that something else already closed ends quietly.
    {
        const ProfileScope closed(profiler, "closed early");
        profiler.endScope();
    }
    EXPECT_EQ(profiler.getDepth(), 0U);
    profiler.endFrame();
    EXPECT_EQ(profiler.getLastFrame().size(), 3U);
}

TEST(ProfilerTest, KeepsARingOfFrameTimes) {
    double now = 0.0;
    Profiler profiler(4, [&now] { return now; });
    EXPECT_EQ(profiler.getAverageFrameMilliseconds(), 0.0);
    EXPECT_TRUE(profiler.getFrameHistory().empty());

    for (int frame = 1; frame <= 6; ++frame) {
        profiler.beginFrame();
        now += 0.001 * frame;
        profiler.endFrame();
    }
    const std::vector<float> history = profiler.getFrameHistory();
    ASSERT_EQ(history.size(), 4U);
    EXPECT_NEAR(history.front(), 3.0F, 1e-4F);
    EXPECT_NEAR(history.back(), 6.0F, 1e-4F);
    EXPECT_NEAR(profiler.getAverageFrameMilliseconds(), 4.5, 1e-4);

    // A scope an error left open closes with the frame.
    profiler.beginFrame();
    profiler.beginScope("broken");
    now += 0.002;
    profiler.endFrame();
    EXPECT_EQ(profiler.getLastFrame().front().name, "broken");
    EXPECT_NEAR(profiler.getLastFrame().front().milliseconds, 2.0, 1e-6);

    EXPECT_THROW(profiler.endScope(), std::logic_error);
    EXPECT_THROW(profiler.endFrame(), std::logic_error);
    EXPECT_THROW(Profiler(0), std::invalid_argument);
}

TEST(LogTest, FansLinesOutToListeners) {
    std::vector<std::pair<core::Log::Level, std::string>> lines;
    const std::uint64_t id = core::Log::addListener([&lines](core::Log::Level level, std::string_view line) { lines.emplace_back(level, std::string(line)); });
    core::Log::warning("storm at {}", "night");
    core::Log::write(core::Log::Level::Error, "fire went out");
    core::Log::removeListener(id);
    core::Log::info("unheard");

    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[0].first, core::Log::Level::Warning);
    EXPECT_NE(lines[0].second.find("storm at night"), std::string::npos);
    EXPECT_EQ(lines[1].first, core::Log::Level::Error);
    EXPECT_NE(lines[1].second.find("fire went out"), std::string::npos);
}

TEST(LogTest, RemovingAListenerWaitsForTheLineItIsWriting) {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
    // clang-format off
    const std::uint64_t id = core::Log::addListener([&](core::Log::Level, std::string_view) {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
    });
    // clang-format on
    std::thread writer([] { core::Log::warning("a line from another thread"); });
    {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return entered; });
    }

    // A plugin removes its listener before it goes away, so the removal must not return while the listener still runs.
    std::atomic<bool> removed = false;
    // clang-format off
    std::thread remover([&] {
        core::Log::removeListener(id);
        removed = true;
    });
    // clang-format on
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(removed);
    {
        const std::scoped_lock lock(mutex);
        released = true;
    }
    changed.notify_all();
    writer.join();
    remover.join();
    EXPECT_TRUE(removed);
}

// An exit() on another thread, such as the one the iOS simulator calls when its render server dies, runs the destructors of static objects while the frame thread still logs and counts objects. The exit handler, registered before the loop first adds a listener, runs after the listeners would have been destroyed and lets the loop go on for a while.
TEST(LogTest, KeepsWorkingWhileAnotherThreadExits) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    // clang-format off
    EXPECT_EXIT({
        static std::atomic<int> iterations = 0;
        std::atexit([] {
            const int seen = iterations.load();
            while (iterations.load() < seen + 1000) {
                std::this_thread::yield();
            }
        });
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::exit(0);
        }).detach();
        for (;;) {
            core::Log::removeListener(core::Log::addListener([](core::Log::Level, std::string_view) {}));
            const ObjectCounter counter("Probe", ObjectCounter::Kind::Native);
            ++iterations;
        }
    }, testing::ExitedWithCode(0), "");
    // clang-format on
}

TEST(DebugPluginTest, CyclesTheStatisticsAndKeepsRecentLines) {
    test::EngineFixture fixture;
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    core::Log::warning("wolves nearby");
    const std::vector<LogLine> recent = plugin.getRecentLog();
    ASSERT_FALSE(recent.empty());
    EXPECT_EQ(recent.back().level, core::Log::Level::Warning);
    EXPECT_NE(recent.back().text.find("wolves nearby"), std::string::npos);

    // clang-format off
    const auto press = [&](input::Key key) {
        platform::Event event;
        event.type = platform::Event::Type::KeyDown;
        event.key = key;
        fixture.engine().handleEvent(event);
    };
    // clang-format on
    EXPECT_EQ(plugin.getStatsMode(), StatsDisplay::Mode::Off);
    press(input::Key::F3);
    EXPECT_EQ(plugin.getStatsMode(), StatsDisplay::Mode::Compact);
    fixture.frames(3);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().instances, 0U);
    press(input::Key::F3);
    EXPECT_EQ(plugin.getStatsMode(), StatsDisplay::Mode::Full);
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().vertices, 0U);

    plugin.setToggleKey(input::Key::F4);
    press(input::Key::F3);
    EXPECT_EQ(plugin.getStatsMode(), StatsDisplay::Mode::Full);
    press(input::Key::F4);
    EXPECT_EQ(plugin.getStatsMode(), StatsDisplay::Mode::Off);
    plugin.setToggleKey(std::nullopt);
    press(input::Key::F4);
    EXPECT_EQ(plugin.getStatsMode(), StatsDisplay::Mode::Off);
    EXPECT_EQ(plugin.getToggleKey(), std::nullopt);

    for (std::size_t line = 0; line < plugins::DebugPlugin::kLogLines + 20; ++line) {
        core::Log::info("line {}", line);
    }
    EXPECT_EQ(plugin.getRecentLog().size(), plugins::DebugPlugin::kLogLines);
}

TEST(DebugPluginTest, ProfilesFromLua) {
    test::EngineFixture fixture;
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
    EXPECT_EQ(fixture.lua("return tostring(debugging.hotReloadWatching())"), "false");
    EXPECT_NE(fixture.lua("debugging.setToggleKey('nope')").find("error: "), std::string::npos);
    EXPECT_EQ(fixture.lua("require('haylen.log').warning('from lua') local lines = debugging.recentLog(1) return #lines .. ' ' .. lines[1].level .. ' ' .. tostring(lines[1].text:find('from lua') ~= nil)"), "1 warning true");
    EXPECT_NE(fixture.lua("debugging.recentLog(-1)").find("expected a non-negative integer"), std::string::npos);
    EXPECT_EQ(fixture.lua("debugging.beginScope('manual') debugging.endScope() return 'ok'"), "ok");
    EXPECT_NE(fixture.lua("debugging.endScope()").find("closed without being opened"), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.profile('bad', function() error('inside') end)").find("inside"), std::string::npos);

    // An app that closes more scopes than it opened ends a scope of the engine early, and the frame goes on.
    fixture.runLua("local scene = require('haylen.scene') scene.clear() scene.push({update = function() debugging.endScope() debugging.profile('work', debugging.endScope) end})");
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::debug
