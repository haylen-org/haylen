#include <gtest/gtest.h>

#include <string>

#include "haylen/input/GestureRecognizer.hpp"
#include "haylen/platform/Event.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::input {

TEST(GestureLuaTest, ListsGesturesOfTheFrame) {
    test::LuaFixture fixture;
    fixture.runLua("input = require('haylen.input') seen = {} require('haylen.scene').push({update = function() for _, g in ipairs(input.gestures()) do seen[#seen + 1] = string.format('%s@%g,%g:%g,%g*%g/%.2f', g.type, g.x, g.y, g.dx, g.dy, g.scale, g.rotation) end end})");
    // clang-format off
    const auto send = [&](haylen::platform::Event::Type type, std::initializer_list<haylen::platform::TouchPoint> touches) {
        haylen::platform::Event event;
        event.type = type;
        for (const haylen::platform::TouchPoint& touch : touches) {
            event.touches[event.touchCount++] = touch;
        }

        fixture.engine().handleEvent(event);
        fixture.frames(1);
    };
    // clang-format on
    send(haylen::platform::Event::Type::TouchBegan, {{.id = 7, .position = {400.0F, 300.0F}, .changed = true}});
    send(haylen::platform::Event::Type::TouchEnded, {{.id = 7, .position = {400.0F, 300.0F}, .changed = true}});
    EXPECT_EQ(fixture.lua("return table.concat(seen, ' ')"), "tap@400,300:0,0*1/0.00");
    send(haylen::platform::Event::Type::TouchBegan, {{.id = 8, .position = {400.0F, 300.0F}, .changed = true}, {.id = 9, .position = {500.0F, 300.0F}, .changed = true}});
    send(haylen::platform::Event::Type::TouchMoved, {{.id = 9, .position = {400.0F, 500.0F}, .changed = true}});
    EXPECT_EQ(fixture.lua("return seen[2]"), "pinch@400,400:-50,100*2/1.57");
    EXPECT_EQ(fixture.lua("input.setGestureSettings({longPressDuration = 1, mouse = false}) return 'ok'"), "ok");
    EXPECT_NE(fixture.lua("input.setGestureSettings({hold = 1})").find("Unknown option \"hold\""), std::string::npos);
    EXPECT_NE(fixture.lua("input.setGestureSettings({tapMaxMovement = -1})").find("The gesture setting \"tapMaxMovement\" must be a finite number from 0."), std::string::npos);
    EXPECT_NE(fixture.lua("input.setGestureSettings({swipeMaxDuration = 0 / 0})").find("The gesture setting \"swipeMaxDuration\" must be a finite number from 0."), std::string::npos);
    EXPECT_EQ(fixture.lua("return tostring(input.gestureSettings().tapMaxMovement == 24)"), "true");
    EXPECT_EQ(haylen::input::Gesture::typeName(haylen::input::Gesture::Type::LongPress), "longPress");
}

} // namespace haylen::lua::input
