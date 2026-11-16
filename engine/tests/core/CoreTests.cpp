#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/ConnectionScope.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/JsonBytes.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/core/Utf8.hpp"

namespace haylen::core {

TEST(FrameClockTest, AccumulatesFixedStepsAndClampsSpikes) {
    FrameClock clock(0.1, 0.25);
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
    FrameClock clock;
    clock.setTimeScale(0.5);
    clock.advance(0.1);
    EXPECT_DOUBLE_EQ(clock.getDelta(), 0.05);
    EXPECT_DOUBLE_EQ(clock.getUnscaledDelta(), 0.1);
    EXPECT_DOUBLE_EQ(clock.getElapsed(), 0.05);
    EXPECT_DOUBLE_EQ(clock.getTimeScale(), 0.5);

    clock.setTimeScale(-2.0);
    EXPECT_DOUBLE_EQ(clock.getTimeScale(), 0.0);
}

// A huge finite scale owes more fixed steps than a frame runs, and the frame drops the rest so it ends.
TEST(FrameClockTest, RunsABoundedNumberOfFixedStepsAtHugeTimeScales) {
    FrameClock clock;
    clock.setTimeScale(1.0e18);
    clock.advance(1.0 / 60.0);
    std::uint32_t steps = 0;
    while (clock.consumeFixedStep()) {
        ++steps;
    }

    EXPECT_EQ(steps, FrameClock::kMaxFixedSteps);
    EXPECT_LT(clock.getInterpolation(), 1.0);
}

TEST(FrameClockTest, RefusesTimeScalesThatAreNotFinite) {
    FrameClock clock;
    clock.setTimeScale(2.0);
    EXPECT_THROW(clock.setTimeScale(std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_THROW(clock.setTimeScale(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    EXPECT_DOUBLE_EQ(clock.getTimeScale(), 2.0);
}

TEST(FrameClockTest, PauseStopsFixedStepsAndGatesProcessModes) {
    FrameClock clock(0.1, 1.0);
    clock.setPaused(true);
    clock.advance(0.35);
    EXPECT_FALSE(clock.consumeFixedStep());
    EXPECT_DOUBLE_EQ(clock.getDelta(), 0.35);
    EXPECT_FALSE(clock.canProcess(ProcessMode::Inherit));
    EXPECT_FALSE(clock.canProcess(ProcessMode::Pausable));
    EXPECT_TRUE(clock.canProcess(ProcessMode::WhenPaused));
    EXPECT_TRUE(clock.canProcess(ProcessMode::Always));
    EXPECT_FALSE(clock.canProcess(ProcessMode::Disabled));

    // Time that passed while paused never turns into a burst of fixed steps.
    clock.setPaused(false);
    EXPECT_TRUE(clock.canProcess(ProcessMode::Pausable));
    EXPECT_FALSE(clock.canProcess(ProcessMode::WhenPaused));
    clock.advance(0.1);
    int steps = 0;
    while (clock.consumeFixedStep()) {
        ++steps;
    }

    EXPECT_EQ(steps, 1);
}

TEST(FrameClockTest, SkipsTheFirstDeltaAfterAResume) {
    FrameClock clock(0.1, 10.0);
    clock.skipNextDelta();
    clock.advance(5.0);
    EXPECT_DOUBLE_EQ(clock.getUnscaledDelta(), 0.0);
    EXPECT_FALSE(clock.consumeFixedStep());
    clock.advance(0.2);
    EXPECT_DOUBLE_EQ(clock.getDelta(), 0.2);
}

// Runs timers on a clock that accepts long frames, so a test can jump through time in one step.
class TimerSchedulerTest : public ::testing::Test {
  protected:
    void step(double seconds) {
        clock.advance(seconds);
        timers.update(clock);
    }

    TimerScheduler timers;
    FrameClock clock{0.1, 10.0};
};

TEST_F(TimerSchedulerTest, FiresDelayedAndRepeatingTimers) {
    int once = 0;
    int repeated = 0;
    timers.after(0.5F, [&] { ++once; });
    const TimerScheduler::Id repeating = timers.every(0.25F, [&] { ++repeated; }, 3);

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

TEST_F(TimerSchedulerTest, PauseCancelAndClear) {
    int calls = 0;
    const TimerScheduler::Id id = timers.every(0.1F, [&] { ++calls; });
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

TEST_F(TimerSchedulerTest, CallbacksCanScheduleAndCancelTimers) {
    std::vector<std::string> events;
    TimerScheduler::Id pendingId = 0;

    // clang-format off
    timers.after(0.1F, [&] {
        events.emplace_back("first");
        pendingId = timers.after(0.1F, [&] { events.emplace_back("second"); });
        EXPECT_TRUE(timers.isActive(pendingId));
    });
    // clang-format on

    const TimerScheduler::Id cancelled = timers.after(0.1F, [&] { events.emplace_back("cancelled"); });
    timers.after(0.05F, [&, cancelled] { timers.cancel(cancelled); });

    step(0.2);
    EXPECT_EQ(events, (std::vector<std::string>{"first"}));

    step(0.2);
    EXPECT_EQ(events, (std::vector<std::string>{"first", "second"}));
}

TEST_F(TimerSchedulerTest, ClearingFromACallbackDropsEveryTimerWithTheOnesItCreated) {
    int calls = 0;
    TimerScheduler::Id created = 0;
    // clang-format off
    timers.after(0.1F, [&] {
        created = timers.after(0.1F, [&calls] { ++calls; });
        timers.clear();
    });
    // clang-format on

    timers.every(0.15F, [&calls] { calls += 10; });

    step(0.2);
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(timers.isActive(created));
    EXPECT_EQ(timers.size(), 0U);
    step(1.0);
    EXPECT_EQ(calls, 0);
}

TEST_F(TimerSchedulerTest, ZeroIntervalRepeatsOncePerUpdate) {
    int calls = 0;
    timers.every(0.0F, [&] { ++calls; });
    step(1.0);
    step(1.0);
    EXPECT_EQ(calls, 2);
}

// An interval too small to pay back a frame would repeat without end, so the timer drops what it owes past the calls of one update.
TEST_F(TimerSchedulerTest, TinyIntervalsRunABoundedNumberOfTimesPerUpdate) {
    std::uint32_t calls = 0;
    timers.every(1.0e-10F, [&] { ++calls; });
    step(1.0 / 60.0);
    EXPECT_EQ(calls, TimerScheduler::kMaxCallsPerUpdate);
    step(1.0 / 60.0);
    EXPECT_EQ(calls, TimerScheduler::kMaxCallsPerUpdate * 2);
}

TEST_F(TimerSchedulerTest, ProcessModesAndUnscaledTime) {
    std::vector<std::string> fired;
    timers.after(0.5F, [&] { fired.emplace_back("pausable"); });
    timers.after(0.5F, [&] { fired.emplace_back("menu"); }, {.processMode = ProcessMode::WhenPaused});
    timers.after(0.5F, [&] { fired.emplace_back("always"); }, {.processMode = ProcessMode::Always, .unscaled = true});
    timers.after(0.5F, [&] { fired.emplace_back("disabled"); }, {.processMode = ProcessMode::Disabled});

    // While paused with the time scale at zero, only the unscaled timer that always runs and the one that runs when paused move, and only the unscaled one makes progress.
    clock.setPaused(true);
    clock.setTimeScale(0.0);
    step(0.6);
    EXPECT_EQ(fired, (std::vector<std::string>{"always"}));

    clock.setTimeScale(1.0);
    step(0.6);
    EXPECT_EQ(fired, (std::vector<std::string>{"always", "menu"}));

    clock.setPaused(false);
    step(0.6);
    EXPECT_EQ(fired, (std::vector<std::string>{"always", "menu", "pausable"}));
    EXPECT_EQ(timers.size(), 1U);
}

TEST_F(TimerSchedulerTest, InheritingTimersFollowTheModeOfTheirParent) {
    ProcessMode parent = ProcessMode::Pausable;
    int calls = 0;
    timers.every(0.1F, [&] { ++calls; }, -1, {.parentMode = [&parent] { return parent; }});
    timers.every(0.1F, [&] { calls += 100; }, -1, {.processMode = ProcessMode::Pausable, .parentMode = [] { return ProcessMode::Always; }});

    clock.setPaused(true);
    step(0.1);
    EXPECT_EQ(calls, 0);

    parent = ProcessMode::WhenPaused;
    step(0.1);
    EXPECT_EQ(calls, 1) << "An own mode wins over the parent.";
}

TEST_F(TimerSchedulerTest, ConnectionsCancelAndBlockTimers) {
    int calls = 0;
    const TimerScheduler::Id id = timers.every(0.1F, [&] { ++calls; });
    Connection connection = timers.getConnection(id);
    EXPECT_TRUE(connection.isConnected());

    connection.setBlocked(true);
    EXPECT_TRUE(connection.isBlocked());
    step(0.5);
    EXPECT_EQ(calls, 0);

    connection.setBlocked(false);
    step(0.1);
    EXPECT_EQ(calls, 1);

    {
        ConnectionScope scope;
        scope.add(connection);
    }

    EXPECT_FALSE(timers.isActive(id));
    EXPECT_FALSE(connection.isConnected());
    EXPECT_FALSE(timers.getConnection(12345).isConnected());
}

TEST(Utf8Test, DecodesValidSequences) {
    EXPECT_EQ(Utf8::decode("aé€😀"), (std::u32string{U'a', U'é', U'€', U'\U0001F600'}));
    EXPECT_EQ(Utf8::countCodePoints("aé€😀"), 4U);
}

TEST(Utf8Test, ReplacesInvalidSequences) {
    EXPECT_EQ(Utf8::decode("\xFF"), std::u32string(1, Utf8::kReplacementCharacter));
    EXPECT_EQ(Utf8::decode("\xE2\x82"), std::u32string(1, Utf8::kReplacementCharacter));
    EXPECT_EQ(Utf8::decode("\xC0\x80"), std::u32string(1, Utf8::kReplacementCharacter));
    EXPECT_EQ(Utf8::decode("\xED\xA0\x80"), std::u32string(1, Utf8::kReplacementCharacter));
}

TEST(Utf8Test, ValidatesWellFormedText) {
    EXPECT_TRUE(Utf8::isValid(""));
    EXPECT_TRUE(Utf8::isValid("aé€😀"));
    EXPECT_TRUE(Utf8::isValid("\xEF\xBF\xBD")) << "An encoded replacement character is valid text.";

    for (const std::string_view invalid : {"\xFF", "\xE2\x82", "a\x80", "\xC0\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xEF\xBF"}) {
        EXPECT_FALSE(Utf8::isValid(invalid));
    }
}

TEST(Utf8Test, EncodesEveryLength) {
    std::string text;
    for (const char32_t codePoint : {U'a', U'é', U'€', U'\U0001F600', static_cast<char32_t>(0xD800), static_cast<char32_t>(0x110000)}) {
        Utf8::append(text, codePoint);
    }

    EXPECT_EQ(Utf8::decode(text), (std::u32string{U'a', U'é', U'€', U'\U0001F600', Utf8::kReplacementCharacter, Utf8::kReplacementCharacter}));
}

TEST(JsonBytesTest, RefersToBuffersOnlyThroughIndexesOfZeroOrMore) {
    EXPECT_EQ(JsonBytes::findReference(Json{{JsonBytes::kKey, 2}}), 2U);

    for (const Json& value : {Json{{JsonBytes::kKey, -1}}, Json{{JsonBytes::kKey, "0"}}, Json{{JsonBytes::kKey, 1}, {"name", "photo"}}, Json::array({0})}) {
        EXPECT_FALSE(JsonBytes::findReference(value).has_value()) << value.dump();
    }

    // A value that only looks like a reference is plain JSON, which a check of the buffers and a shift leave as it is.
    Json document = {{"negative", {{JsonBytes::kKey, -1}}}, {"photo", JsonBytes::makeReference(0)}};
    EXPECT_NO_THROW(JsonBytes::validate(document, 1));
    JsonBytes::shift(document, 3);
    EXPECT_EQ(document.at("negative").at(JsonBytes::kKey), -1);
    EXPECT_EQ(JsonBytes::findReference(document.at("photo")), 3U);
}

TEST(JsonNumberTest, WritesFloatsWithTheirShortestDecimalForm) {
    EXPECT_EQ(Json(JsonNumber::fromFloat(0.6F)).dump(), "0.6");
    EXPECT_EQ(Json::array({JsonNumber::fromFloat(0.1F), JsonNumber::fromFloat(-1.25F), JsonNumber::fromFloat(1e-7F), JsonNumber::fromFloat(3.4028235e38F), JsonNumber::fromFloat(1920.0F)}).dump(), "[0.1,-1.25,1e-07,3.4028235e+38,1920.0]");
    EXPECT_EQ(static_cast<float>(JsonNumber::fromFloat(0.3F)), 0.3F);
}

// A number of a document reads as a float only when a float holds it, since converting a larger one is undefined.
TEST(JsonNumberTest, ReadsOnlyTheNumbersThatAFloatHolds) {
    EXPECT_EQ(JsonNumber::toFloat(Json(-3.4e38)), -3.4e38F);
    EXPECT_EQ(JsonNumber::toFloat(Json(7)), 7.0F);
    EXPECT_EQ(JsonNumber::toFloat(Json{{"speed", 2.5}}, "speed", 1.0F), 2.5F);
    EXPECT_EQ(JsonNumber::toFloat(Json::object(), "speed", 1.0F), 1.0F);

    try {
        (void)JsonNumber::toFloat(Json{{"speed", -1e39}}, "speed", 1.0F);
        FAIL() << "A number past the floats must be refused.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The number -1e+39 lies outside the numbers from -3.4028234663853e+38 to 3.4028234663853e+38 that the engine holds.");
    }
}

} // namespace haylen::core
