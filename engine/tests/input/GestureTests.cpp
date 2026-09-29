#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/GestureRecognizer.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/platform/Event.hpp"
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
    EXPECT_EQ(frame(), (Names{"tap", "double_tap"}));
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

TEST_F(GestureTest, RecognizesLongPressesAndSwipes) {
    touch(platform::Event::Type::TouchBegan, {{1, {100.0F, 100.0F}}});
    Names seen;
    for (int step = 0; step < 14; ++step) {
        for (const std::string& name : frame()) {
            seen.push_back(name);
        }
    }
    EXPECT_EQ(seen, Names{"long_press"});
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
    fixture.runLua("input = require('haylen.input') seen = {} require('haylen.scene').push({update = function() for _, g in ipairs(input.gestures()) do seen[#seen + 1] = g.type .. '@' .. g.x .. ',' .. g.y end end})");
    // clang-format off
    const auto send = [&](platform::Event::Type type) {
        platform::Event event;
        event.type = type;
        event.touchCount = 1;
        event.touches[0] = {.id = 7, .position = {400.0F, 300.0F}, .changed = true};
        fixture.engine().handleEvent(event);
        fixture.frames(1);
    };
    // clang-format on
    send(platform::Event::Type::TouchBegan);
    send(platform::Event::Type::TouchEnded);
    EXPECT_EQ(fixture.lua("return table.concat(seen, ' ')"), "tap@400.0,300.0");
    EXPECT_EQ(fixture.lua("input.setGestureSettings({longPressDuration = 1, mouse = false}) return 'ok'"), "ok");
    EXPECT_NE(fixture.lua("input.setGestureSettings({hold = 1})").find("Unknown option 'hold'"), std::string::npos);
    EXPECT_EQ(Gesture::typeName(Gesture::Type::LongPress), "long_press");
}

} // namespace haylen::input
