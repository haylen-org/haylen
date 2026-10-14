#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/GestureRecognizer.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/platform/Event.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::input {

namespace {

class GestureTest : public ::testing::Test {
  protected:
    GestureTest() {
        viewport.update({1920.0F, 1080.0F}, {1920.0F, 1080.0F}, graphics::Viewport::ScalingPolicy::Fit);
    }

    void touch(platform::Event::Type type, std::initializer_list<std::pair<std::uint64_t, math::Vec2>> points) {
        platform::Event event;
        event.type = type;
        for (const auto& [id, position] : points) {
            event.touches[event.touchCount++] = {.id = id, .position = position, .changed = true};
        }
        input.handleEvent(event, viewport);
    }

    void mouse(platform::Event::Type type, math::Vec2 position) {
        platform::Event event;
        event.type = type;
        event.position = position;
        input.handleEvent(event, viewport);
    }

    // Runs one frame and returns the names of what was recognized.
    std::vector<std::string> frame(float seconds = 0.05F) {
        input.updateTouchDurations(seconds);
        gestures.update(input, seconds);
        std::vector<std::string> names;
        for (const Gesture& gesture : gestures.getGestures()) {
            names.emplace_back(Gesture::typeName(gesture.type));
        }
        input.endFrame();
        return names;
    }

    [[nodiscard]] const Gesture& first() const {
        return gestures.getGestures().front();
    }

    graphics::Viewport viewport;
    Input input;
    GestureRecognizer gestures;
};

using Names = std::vector<std::string>;

} // namespace

TEST_F(GestureTest, RecognizesTapsAndDoubleTaps) {
    touch(platform::Event::Type::TouchBegan, {{1, {100.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{});
    touch(platform::Event::Type::TouchEnded, {{1, {104.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{"tap"});

    touch(platform::Event::Type::TouchBegan, {{2, {110.0F, 100.0F}}});
    frame();
    touch(platform::Event::Type::TouchEnded, {{2, {110.0F, 102.0F}}});
    EXPECT_EQ(frame(), (Names{"tap", "doubleTap"}));
    EXPECT_EQ(gestures.getGestures()[1].position, math::Vec2(110.0F, 102.0F));

    // A third tap starts a new pair, and a tap that lands and lifts in one frame still counts from where it began.
    touch(platform::Event::Type::TouchBegan, {{3, {300.0F, 300.0F}}});
    touch(platform::Event::Type::TouchEnded, {{3, {300.0F, 300.0F}}});
    EXPECT_EQ(frame(), Names{"tap"});
    for (int step = 0; step < 10; ++step) {
        frame();
    }
    touch(platform::Event::Type::TouchBegan, {{4, {300.0F, 300.0F}}});
    touch(platform::Event::Type::TouchEnded, {{4, {300.0F, 300.0F}}});
    EXPECT_EQ(frame(), Names{"tap"});
}

TEST_F(GestureTest, CancelledPressesNeverEndAsGestures) {
    touch(platform::Event::Type::TouchBegan, {{1, {100.0F, 100.0F}}});
    frame();
    touch(platform::Event::Type::TouchCancelled, {{1, {100.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{});

    // Losing focus releases the held button and cancels the fingers, which the player never lifted.
    mouse(platform::Event::Type::MouseDown, {50.0F, 60.0F});
    frame();
    input.releaseAll();
    gestures.cancel();
    EXPECT_EQ(frame(), Names{});

    touch(platform::Event::Type::TouchBegan, {{2, {100.0F, 100.0F}}});
    frame();
    input.releaseAll();
    gestures.cancel();
    EXPECT_EQ(frame(), Names{});
}

TEST_F(GestureTest, AReusedTouchIdStartsANewPointer) {
    touch(platform::Event::Type::TouchBegan, {{1, {100.0F, 100.0F}}});
    frame();
    touch(platform::Event::Type::TouchEnded, {{1, {100.0F, 100.0F}}});
    touch(platform::Event::Type::TouchBegan, {{1, {600.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{"tap"});
    EXPECT_EQ(first().position, math::Vec2(100.0F, 100.0F));

    touch(platform::Event::Type::TouchEnded, {{1, {600.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{"tap"});
    EXPECT_EQ(first().position, math::Vec2(600.0F, 100.0F));
}

TEST_F(GestureTest, RecognizesLongPressesAndSwipes) {
    touch(platform::Event::Type::TouchBegan, {{1, {100.0F, 100.0F}}});
    Names seen;
    for (int step = 0; step < 14; ++step) {
        for (const std::string& name : frame()) {
            seen.push_back(name);
        }
    }
    EXPECT_EQ(seen, Names{"longPress"});
    touch(platform::Event::Type::TouchEnded, {{1, {100.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{});

    touch(platform::Event::Type::TouchBegan, {{2, {100.0F, 100.0F}}});
    frame();
    touch(platform::Event::Type::TouchMoved, {{2, {220.0F, 110.0F}}});
    frame();
    touch(platform::Event::Type::TouchEnded, {{2, {300.0F, 120.0F}}});
    EXPECT_EQ(frame(), Names{"swipe"});
    EXPECT_EQ(first().delta, math::Vec2(200.0F, 20.0F));

    touch(platform::Event::Type::TouchBegan, {{3, {100.0F, 100.0F}}});
    for (int step = 0; step < 12; ++step) {
        frame();
    }
    touch(platform::Event::Type::TouchEnded, {{3, {400.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{});
}

TEST_F(GestureTest, RecognizesPinches) {
    touch(platform::Event::Type::TouchBegan, {{1, {100.0F, 100.0F}}, {2, {200.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{});
    touch(platform::Event::Type::TouchMoved, {{2, {300.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{"pinch"});
    EXPECT_FLOAT_EQ(first().scale, 2.0F);
    EXPECT_EQ(first().position, math::Vec2(200.0F, 100.0F));
    EXPECT_EQ(frame(), Names{});
    touch(platform::Event::Type::TouchEnded, {{1, {100.0F, 100.0F}}, {2, {300.0F, 100.0F}}});
    EXPECT_EQ(frame(), Names{});
}

TEST_F(GestureTest, TurnsAndMovesAPinchWithItsFingers) {
    touch(platform::Event::Type::TouchBegan, {{2, {200.0F, 100.0F}}, {1, {100.0F, 100.0F}}});
    frame();
    touch(platform::Event::Type::TouchMoved, {{1, {150.0F, 150.0F}}, {2, {150.0F, 250.0F}}});
    EXPECT_EQ(frame(), Names{"pinch"});
    EXPECT_NEAR(first().rotation, std::numbers::pi_v<float> * 0.5F, 1.0e-5F);
    EXPECT_FLOAT_EQ(first().scale, 1.0F);
    EXPECT_EQ(first().position, math::Vec2(150.0F, 200.0F));
    EXPECT_EQ(first().delta, math::Vec2(0.0F, 100.0F));

    touch(platform::Event::Type::TouchMoved, {{1, {200.0F, 100.0F}}, {2, {100.0F, 100.0F}}});
    frame();
    EXPECT_NEAR(std::abs(first().rotation), std::numbers::pi_v<float>, 1.0e-5F);
    EXPECT_EQ(first().delta, math::Vec2(0.0F, 0.0F));
}

TEST_F(GestureTest, FollowsAPinchWithoutAllocating) {
    touch(platform::Event::Type::TouchBegan, {{1, {100.0F, 100.0F}}, {2, {200.0F, 100.0F}}});
    frame();
    touch(platform::Event::Type::TouchMoved, {{2, {300.0F, 100.0F}}});
    frame();
    const test::AllocationTracker tracker;
    touch(platform::Event::Type::TouchMoved, {{2, {320.0F, 100.0F}}});
    input.updateTouchDurations(0.05F);
    gestures.update(input, 0.05F);
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(gestures.getGestures().size(), 1U);
}

TEST_F(GestureTest, TreatsTheMouseAsOneFinger) {
    mouse(platform::Event::Type::MouseDown, {50.0F, 60.0F});
    frame();
    mouse(platform::Event::Type::MouseUp, {50.0F, 60.0F});
    EXPECT_EQ(frame(), Names{"tap"});
    EXPECT_EQ(first().position, math::Vec2(50.0F, 60.0F));

    gestures.setSettings({.mouse = false});
    EXPECT_FALSE(gestures.getSettings().mouse);
    mouse(platform::Event::Type::MouseDown, {50.0F, 60.0F});
    frame();
    mouse(platform::Event::Type::MouseUp, {50.0F, 60.0F});
    EXPECT_EQ(frame(), Names{});
}

TEST(GestureLuaTest, ListsGesturesOfTheFrame) {
    test::EngineFixture fixture;
    fixture.runLua("input = require('haylen.input') seen = {} require('haylen.scene').push({update = function() for _, g in ipairs(input.gestures()) do seen[#seen + 1] = string.format('%s@%g,%g:%g,%g*%g/%.2f', g.type, g.x, g.y, g.dx, g.dy, g.scale, g.rotation) end end})");
    // clang-format off
    const auto send = [&](platform::Event::Type type, std::initializer_list<platform::TouchPoint> touches) {
        platform::Event event;
        event.type = type;
        for (const platform::TouchPoint& touch : touches) {
            event.touches[event.touchCount++] = touch;
        }
        fixture.engine().handleEvent(event);
        fixture.frames(1);
    };
    // clang-format on
    send(platform::Event::Type::TouchBegan, {{.id = 7, .position = {400.0F, 300.0F}, .changed = true}});
    send(platform::Event::Type::TouchEnded, {{.id = 7, .position = {400.0F, 300.0F}, .changed = true}});
    EXPECT_EQ(fixture.lua("return table.concat(seen, ' ')"), "tap@400,300:0,0*1/0.00");
    send(platform::Event::Type::TouchBegan, {{.id = 8, .position = {400.0F, 300.0F}, .changed = true}, {.id = 9, .position = {500.0F, 300.0F}, .changed = true}});
    send(platform::Event::Type::TouchMoved, {{.id = 9, .position = {400.0F, 500.0F}, .changed = true}});
    EXPECT_EQ(fixture.lua("return seen[2]"), "pinch@400,400:-50,100*2/1.57");
    EXPECT_EQ(fixture.lua("input.setGestureSettings({longPressDuration = 1, mouse = false}) return 'ok'"), "ok");
    EXPECT_NE(fixture.lua("input.setGestureSettings({hold = 1})").find("Unknown option \"hold\""), std::string::npos);
    EXPECT_EQ(Gesture::typeName(Gesture::Type::LongPress), "longPress");
}

TEST(GestureEngineTest, FocusLossDropsTheHeldPress) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    engine.handleEvent({.type = platform::Event::Type::MouseDown, .position = {400.0F, 300.0F}});
    fixture.frames(1);
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    fixture.frames(1);
    EXPECT_TRUE(engine.getGestures().getGestures().empty());
}

} // namespace haylen::input
