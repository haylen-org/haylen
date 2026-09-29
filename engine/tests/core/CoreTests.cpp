#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/ConnectionScope.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/core/Utf8.hpp"

namespace haylen::core {

TEST(FrameClockTest, AccumulatesFixedStepsAndClampsSpikes) {
    core::FrameClock clock(0.1, 0.25);
    clock.advance(0.35);
    EXPECT_DOUBLE_EQ(clock.getUnscaledDelta(), 0.25);

    int steps = 0;
    while (clock.consumeFixedStep()) {
        ++steps;
    }
    EXPECT_EQ(steps, 2);
    EXPECT_NEAR(clock.getInterpolation(), 0.5, 1e-9);
    EXPECT_EQ(clock.getFrameIndex(), 1U);
    EXPECT_DOUBLE_EQ(clock.getFixedStep(), 0.1);

    clock.advance(-1.0);
    EXPECT_DOUBLE_EQ(clock.getDelta(), 0.0);
}

TEST(FrameClockTest, TimeScaleAffectsScaledTimeOnly) {
    core::FrameClock clock;
    clock.setTimeScale(0.5);
    clock.advance(0.1);
    EXPECT_DOUBLE_EQ(clock.getDelta(), 0.05);
    EXPECT_DOUBLE_EQ(clock.getUnscaledDelta(), 0.1);
    EXPECT_DOUBLE_EQ(clock.getElapsed(), 0.05);
    EXPECT_DOUBLE_EQ(clock.getTimeScale(), 0.5);

    clock.setTimeScale(-2.0);
    EXPECT_DOUBLE_EQ(clock.getTimeScale(), 0.0);
}

TEST(FrameClockTest, PauseStopsFixedStepsAndGatesProcessModes) {
    core::FrameClock clock(0.1, 1.0);
    clock.setPaused(true);
    clock.advance(0.35);
    EXPECT_FALSE(clock.consumeFixedStep());
    EXPECT_DOUBLE_EQ(clock.getDelta(), 0.35);
    EXPECT_FALSE(clock.canProcess(core::ProcessMode::Inherit));
    EXPECT_FALSE(clock.canProcess(core::ProcessMode::Pausable));
    EXPECT_TRUE(clock.canProcess(core::ProcessMode::WhenPaused));
    EXPECT_TRUE(clock.canProcess(core::ProcessMode::Always));
    EXPECT_FALSE(clock.canProcess(core::ProcessMode::Disabled));

    // Time that passed while paused never turns into a burst of fixed steps.
    clock.setPaused(false);
    EXPECT_TRUE(clock.canProcess(core::ProcessMode::Pausable));
    EXPECT_FALSE(clock.canProcess(core::ProcessMode::WhenPaused));
    clock.advance(0.1);
    int steps = 0;
    while (clock.consumeFixedStep()) {
        ++steps;
    }
    EXPECT_EQ(steps, 1);
}

TEST(FrameClockTest, SkipsTheFirstDeltaAfterAResume) {
    core::FrameClock clock(0.1, 10.0);
    clock.skipNextDelta();
    clock.advance(5.0);
    EXPECT_DOUBLE_EQ(clock.getUnscaledDelta(), 0.0);
    EXPECT_FALSE(clock.consumeFixedStep());
    clock.advance(0.2);
    EXPECT_DOUBLE_EQ(clock.getDelta(), 0.2);
}

TEST(TimerSchedulerTest, FiresDelayedAndRepeatingTimers) {
    core::TimerScheduler timers;
    core::FrameClock clock(0.1, 10.0);
    // clang-format off
    const auto step = [&](double seconds) {
        clock.advance(seconds);
        timers.update(clock);
    };
    // clang-format on
    int once = 0;
    int repeated = 0;
    timers.after(0.5F, [&] { ++once; });
    const core::TimerScheduler::Id repeating = timers.every(0.25F, [&] { ++repeated; }, 3);

    step(0.3);
    EXPECT_EQ(once, 0);
    EXPECT_EQ(repeated, 1);

    step(0.3);
    EXPECT_EQ(once, 1);
    EXPECT_EQ(repeated, 2);

    step(1.0);
    EXPECT_EQ(repeated, 3);
    EXPECT_FALSE(timers.isActive(repeating));
    EXPECT_EQ(timers.size(), 0U);
    EXPECT_THROW((void)timers.every(0.25F, [] {}, 0), std::invalid_argument);
    EXPECT_THROW((void)timers.after(0.25F, {}), std::invalid_argument);
}

TEST(TimerSchedulerTest, PauseCancelAndClear) {
    core::TimerScheduler timers;
    core::FrameClock clock(0.1, 10.0);
    // clang-format off
    const auto step = [&](double seconds) {
        clock.advance(seconds);
        timers.update(clock);
    };
    // clang-format on
    int calls = 0;
    const core::TimerScheduler::Id id = timers.every(0.1F, [&] { ++calls; });
    timers.pause(id, true);
    step(1.0);
    EXPECT_EQ(calls, 0);

    timers.pause(id, false);
    step(0.1);
    EXPECT_EQ(calls, 1);

    timers.cancel(id);
    step(1.0);
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(timers.isActive(id));

    timers.after(0.1F, [&] { ++calls; });
    timers.clear();
    step(1.0);
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(timers.isActive(9999));
}

TEST(TimerSchedulerTest, CallbacksCanScheduleAndCancelTimers) {
    core::TimerScheduler timers;
    core::FrameClock clock(0.1, 10.0);
    std::vector<std::string> events;
    core::TimerScheduler::Id pendingId = 0;

    // clang-format off
    timers.after(0.1F, [&] {
        events.emplace_back("first");
        pendingId = timers.after(0.1F, [&] { events.emplace_back("second"); });
        EXPECT_TRUE(timers.isActive(pendingId));
    });
    // clang-format on
    const core::TimerScheduler::Id cancelled = timers.after(0.1F, [&] { events.emplace_back("cancelled"); });
    timers.after(0.05F, [&, cancelled] { timers.cancel(cancelled); });

    clock.advance(0.2);
    timers.update(clock);
    EXPECT_EQ(events, (std::vector<std::string>{"first"}));

    clock.advance(0.2);
    timers.update(clock);
    EXPECT_EQ(events, (std::vector<std::string>{"first", "second"}));
}

TEST(TimerSchedulerTest, ZeroIntervalRepeatsOncePerUpdate) {
    core::TimerScheduler timers;
    core::FrameClock clock(0.1, 10.0);
    int calls = 0;
    timers.every(0.0F, [&] { ++calls; });
    clock.advance(1.0);
    timers.update(clock);
    clock.advance(1.0);
    timers.update(clock);
    EXPECT_EQ(calls, 2);
}

TEST(TimerSchedulerTest, ProcessModesAndUnscaledTime) {
    core::TimerScheduler timers;
    core::FrameClock clock(0.1, 10.0);
    std::vector<std::string> fired;
    timers.after(0.5F, [&] { fired.emplace_back("pausable"); });
    timers.after(0.5F, [&] { fired.emplace_back("menu"); }, {.processMode = core::ProcessMode::WhenPaused});
    timers.after(0.5F, [&] { fired.emplace_back("always"); }, {.processMode = core::ProcessMode::Always, .unscaled = true});
    timers.after(0.5F, [&] { fired.emplace_back("disabled"); }, {.processMode = core::ProcessMode::Disabled});

    // While paused with the time scale at zero, only the unscaled timer that always runs and the one that runs when paused move, and only the unscaled one makes progress.
    clock.setPaused(true);
    clock.setTimeScale(0.0);
    clock.advance(0.6);
    timers.update(clock);
    EXPECT_EQ(fired, (std::vector<std::string>{"always"}));

    clock.setTimeScale(1.0);
    clock.advance(0.6);
    timers.update(clock);
    EXPECT_EQ(fired, (std::vector<std::string>{"always", "menu"}));

    clock.setPaused(false);
    clock.advance(0.6);
    timers.update(clock);
    EXPECT_EQ(fired, (std::vector<std::string>{"always", "menu", "pausable"}));
    EXPECT_EQ(timers.size(), 1U);
}

TEST(TimerSchedulerTest, InheritingTimersFollowTheModeOfTheirParent) {
    core::TimerScheduler timers;
    core::FrameClock clock(0.1, 10.0);
    core::ProcessMode parent = core::ProcessMode::Pausable;
    int calls = 0;
    timers.every(0.1F, [&] { ++calls; }, -1, {.parentMode = [&parent] { return parent; }});
    timers.every(0.1F, [&] { calls += 100; }, -1, {.processMode = core::ProcessMode::Pausable, .parentMode = [] { return core::ProcessMode::Always; }});

    clock.setPaused(true);
    clock.advance(0.1);
    timers.update(clock);
    EXPECT_EQ(calls, 0);

    parent = core::ProcessMode::WhenPaused;
    clock.advance(0.1);
    timers.update(clock);
    EXPECT_EQ(calls, 1) << "an own mode wins over the parent";
}

TEST(TimerSchedulerTest, ConnectionsCancelAndBlockTimers) {
    core::TimerScheduler timers;
    core::FrameClock clock(0.1, 10.0);
    int calls = 0;
    const core::TimerScheduler::Id id = timers.every(0.1F, [&] { ++calls; });
    core::Connection connection = timers.getConnection(id);
    EXPECT_TRUE(connection.isConnected());

    connection.setBlocked(true);
    EXPECT_TRUE(connection.isBlocked());
    clock.advance(0.5);
    timers.update(clock);
    EXPECT_EQ(calls, 0);

    connection.setBlocked(false);
    clock.advance(0.1);
    timers.update(clock);
    EXPECT_EQ(calls, 1);

    {
        core::ConnectionScope scope;
        scope.add(connection);
    }
    EXPECT_FALSE(timers.isActive(id));
    EXPECT_FALSE(connection.isConnected());
    EXPECT_FALSE(timers.getConnection(12345).isConnected());
}

TEST(Utf8Test, DecodesValidSequences) {
    EXPECT_EQ(core::Utf8::decode("aé€😀"), (std::u32string{U'a', U'é', U'€', U'\U0001F600'}));
    EXPECT_EQ(core::Utf8::countCodePoints("aé€😀"), 4U);
}

TEST(Utf8Test, ReplacesInvalidSequences) {
    EXPECT_EQ(core::Utf8::decode("\xFF"), std::u32string(1, core::Utf8::kReplacementCharacter));
    EXPECT_EQ(core::Utf8::decode("\xE2\x82"), std::u32string(1, core::Utf8::kReplacementCharacter));
    EXPECT_EQ(core::Utf8::decode("\xC0\x80"), std::u32string(1, core::Utf8::kReplacementCharacter));
    EXPECT_EQ(core::Utf8::decode("\xED\xA0\x80"), std::u32string(1, core::Utf8::kReplacementCharacter));
}

TEST(Utf8Test, EncodesEveryLength) {
    std::string text;
    for (const char32_t codePoint : {U'a', U'é', U'€', U'\U0001F600', static_cast<char32_t>(0xD800), static_cast<char32_t>(0x110000)}) {
        core::Utf8::append(text, codePoint);
    }
    EXPECT_EQ(core::Utf8::decode(text), (std::u32string{U'a', U'é', U'€', U'\U0001F600', core::Utf8::kReplacementCharacter, core::Utf8::kReplacementCharacter}));
}

TEST(JsonNumberTest, WritesFloatsWithTheirShortestDecimalForm) {
    EXPECT_EQ(Json(JsonNumber::fromFloat(0.6F)).dump(), "0.6");
    EXPECT_EQ(Json::array({JsonNumber::fromFloat(0.1F), JsonNumber::fromFloat(-1.25F), JsonNumber::fromFloat(1e-7F), JsonNumber::fromFloat(3.4028235e38F), JsonNumber::fromFloat(1920.0F)}).dump(), "[0.1,-1.25,1e-07,3.4028235e+38,1920.0]");
    EXPECT_EQ(static_cast<float>(JsonNumber::fromFloat(0.3F)), 0.3F);
}

} // namespace haylen::core
