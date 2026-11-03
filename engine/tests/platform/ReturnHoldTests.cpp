#include <gtest/gtest.h>

#include <cstdint>

#include "haylen/core/Engine.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/ReturnHold.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class ReturnHoldTest : public ::testing::Test {
  protected:
    static Event key(Event::Type type) {
        return {.type = type, .key = input::Key::Escape};
    }

    static Event touch(Event::Type type, std::uint64_t id, math::Vec2 position) {
        Event event{.type = type, .touchCount = 1};
        event.touches[0] = {.id = id, .position = position, .changed = true};
        return event;
    }

    // Advances the hold in steps of a frame for the seconds, and returns whether any step completed it.
    bool hold(float seconds, bool backHeld = false) {
        bool completed = false;
        for (float elapsed = 0.0F; elapsed < seconds; elapsed += kFrame) {
            completed = returnHold.update(kFrame, backHeld) || completed;
        }
        return completed;
    }

    static constexpr float kFrame = 1.0F / 60.0F;
    ReturnHold returnHold;
};

TEST_F(ReturnHoldTest, CompletesOnceForEscapeHeldForTheWholeHold) {
    returnHold.handleEvent(key(Event::Type::KeyDown), 1.0F);
    EXPECT_FALSE(hold(ReturnHold::kSeconds - 0.1F));
    EXPECT_GT(returnHold.getProgress(), 0.8F);
    EXPECT_TRUE(hold(0.2F));
    EXPECT_FALSE(hold(1.0F));

    // A release starts the next hold from nothing.
    returnHold.handleEvent(key(Event::Type::KeyUp), 1.0F);
    EXPECT_FALSE(hold(0.1F));
    EXPECT_EQ(returnHold.getProgress(), 0.0F);
}

TEST_F(ReturnHoldTest, IgnoresShortPressesAndKeysThatAWindowStoppedHearing) {
    for (int press = 0; press < 5; ++press) {
        returnHold.handleEvent(key(Event::Type::KeyDown), 1.0F);
        EXPECT_FALSE(hold(0.5F));
        returnHold.handleEvent(key(Event::Type::KeyUp), 1.0F);
        EXPECT_FALSE(hold(0.1F));
    }
    returnHold.handleEvent(key(Event::Type::KeyDown), 1.0F);
    returnHold.handleEvent({.type = Event::Type::FocusLost}, 1.0F);
    EXPECT_FALSE(hold(ReturnHold::kSeconds * 2.0F));
}

// The play and pause button of a TV remote presses the key "pause", whose hold leaves too, since the Apple TV takes a hold of Menu to its Home screen.
TEST_F(ReturnHoldTest, CompletesForPlayAndPauseOfATvRemoteHeld) {
    returnHold.handleEvent({.type = Event::Type::KeyDown, .key = input::Key::Pause}, 1.0F);
    EXPECT_TRUE(hold(ReturnHold::kSeconds + 0.1F));
    returnHold.handleEvent({.type = Event::Type::KeyUp, .key = input::Key::Pause}, 1.0F);
    EXPECT_FALSE(hold(ReturnHold::kSeconds + 0.1F));
}

TEST_F(ReturnHoldTest, CompletesForTheBackButtonHeld) {
    EXPECT_FALSE(hold(ReturnHold::kSeconds * 0.5F, true));
    EXPECT_TRUE(hold(ReturnHold::kSeconds * 0.6F, true));
}

TEST_F(ReturnHoldTest, CompletesOnlyForThreeFingersHeldStill) {
    for (std::uint64_t id = 1; id <= 2; ++id) {
        returnHold.handleEvent(touch(Event::Type::TouchBegan, id, {100.0F * static_cast<float>(id), 100.0F}), 2.0F);
    }
    EXPECT_FALSE(hold(ReturnHold::kSeconds * 2.0F));

    // A finger that travels past the slop at the scale of the screen turns the hold into a gesture of the app.
    returnHold.handleEvent(touch(Event::Type::TouchBegan, 3, {300.0F, 100.0F}), 2.0F);
    returnHold.handleEvent(touch(Event::Type::TouchMoved, 3, {300.0F + ReturnHold::kFingerTravel * 1.5F, 100.0F}), 2.0F);
    EXPECT_FALSE(hold(0.5F));
    returnHold.handleEvent(touch(Event::Type::TouchMoved, 3, {300.0F + ReturnHold::kFingerTravel * 2.5F, 100.0F}), 2.0F);
    EXPECT_FALSE(hold(ReturnHold::kSeconds * 2.0F));

    returnHold.handleEvent(touch(Event::Type::TouchEnded, 3, {}), 2.0F);
    returnHold.handleEvent(touch(Event::Type::TouchBegan, 4, {300.0F, 100.0F}), 2.0F);
    EXPECT_TRUE(hold(ReturnHold::kSeconds + 0.1F));
}

// Only an app that another app launched leaves through the hold, which every other app hears as plain input.
TEST(ReturnHoldPluginTest, LeavesOnlyALaunchedApp) {
    test::EngineFixture fixture;
    fixture.engine().handleEvent({.type = Event::Type::KeyDown, .key = input::Key::Escape});
    fixture.frames(120);
    EXPECT_TRUE(fixture.engine().isRunning());

    fixture.engine().handleEvent({.type = Event::Type::KeyUp, .key = input::Key::Escape});
    fixture.host().setLaunched(true);
    fixture.engine().handleEvent({.type = Event::Type::KeyDown, .key = input::Key::Escape});
    fixture.frames(60);
    EXPECT_TRUE(fixture.engine().isRunning());
    fixture.frames(60);
    EXPECT_FALSE(fixture.engine().isRunning());
}

} // namespace haylen::platform
