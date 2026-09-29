#include <gtest/gtest.h>

#include <string>

#include "haylen/input/Input.hpp"
#include "haylen/platform/Event.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::input {

class InputLuaTest : public ::testing::Test {
  protected:
    [[nodiscard]] static platform::Event makeKeyEvent(platform::Event::Type type, Key key) {
        return {.type = type, .key = key, .modifiers = {.shift = true}};
    }

    [[nodiscard]] static platform::Event makeMouseEvent(platform::Event::Type type, math::Vec2 position, MouseButton button = MouseButton::Left) {
        return {.type = type, .mouseButton = button, .position = position};
    }

    [[nodiscard]] static platform::Event makeTouchEvent(platform::Event::Type type, math::Vec2 position) {
        platform::Event event{.type = type, .touchCount = 1};
        event.touches[0] = {.id = 7, .position = position, .changed = true};
        return event;
    }

    void SetUp() override {
        fixture.runLua("input = require('haylen.input')");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::EngineFixture fixture;
};

TEST_F(InputLuaTest, ReadsKeyboardState) {
    core::Engine& engine = fixture.engine();
    engine.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::Space));
    engine.handleEvent({.type = platform::Event::Type::Character, .character = U'é'});
    EXPECT_EQ(lua("return input.keyDown('space') and input.keyPressed('space') and input.anyKeyPressed()"), "true");
    EXPECT_EQ(lua("return input.modifiers().shift and not input.modifiers().control"), "true");
    EXPECT_EQ(lua("return input.text()"), "é");
    EXPECT_EQ(lua("return input.lastDevice()"), "keyboard_mouse");

    fixture.frames(1);
    EXPECT_EQ(lua("return input.keyDown('space') and not input.keyPressed('space')"), "true");
    engine.handleEvent(makeKeyEvent(platform::Event::Type::KeyUp, Key::Space));
    EXPECT_EQ(lua("return input.keyReleased('space') and not input.keyDown('space')"), "true");
    EXPECT_NE(lua("return input.keyDown('hyperspace')").find("error: "), std::string::npos);
}

TEST_F(InputLuaTest, ReadsMouseInDesignCoordinates) {
    core::Engine& engine = fixture.engine();
    fixture.host().resize({960.0F, 540.0F});
    fixture.frames(1);

    engine.handleEvent(makeMouseEvent(platform::Event::Type::MouseMove, {100.0F, 50.0F}));
    engine.handleEvent(makeMouseEvent(platform::Event::Type::MouseDown, {100.0F, 50.0F}, MouseButton::Right));
    engine.handleEvent({.type = platform::Event::Type::MouseScroll, .scroll = {0.0F, -2.0F}});
    EXPECT_EQ(lua("local x, y = input.mousePosition() return x .. ',' .. y"), "200.0,100.0");
    EXPECT_EQ(lua("local x, y = input.mouseDelta() return x .. ',' .. y"), "200.0,100.0");
    EXPECT_EQ(lua("local x, y = input.mouseScroll() return x .. ',' .. y"), "0.0,-2.0");
    EXPECT_EQ(lua("return input.mouseDown('right') and input.mousePressed('right') and not input.mouseDown()"), "true");
    EXPECT_EQ(lua("return input.mouseInside()"), "true");

    engine.handleEvent(makeMouseEvent(platform::Event::Type::MouseUp, {100.0F, 50.0F}, MouseButton::Right));
    engine.handleEvent({.type = platform::Event::Type::MouseLeave});
    EXPECT_EQ(lua("return input.mouseReleased('right') and not input.mouseInside()"), "true");
}

TEST_F(InputLuaTest, TracksTouches) {
    core::Engine& engine = fixture.engine();
    engine.handleEvent(makeTouchEvent(platform::Event::Type::TouchBegan, {100.0F, 200.0F}));
    EXPECT_EQ(lua("local t = input.touches()[1] return t.id .. ' ' .. t.x .. ' ' .. t.phase"), "7 100.0 began");
    EXPECT_EQ(lua("return input.lastDevice()"), "touch");

    fixture.frames(1);
    engine.handleEvent(makeTouchEvent(platform::Event::Type::TouchMoved, {130.0F, 200.0F}));
    EXPECT_EQ(lua("local t = input.touches()[1] return t.dx .. ' ' .. t.startX .. ' ' .. t.phase .. ' ' .. tostring(t.duration > 0)"), "30.0 100.0 moved true");

    engine.handleEvent(makeTouchEvent(platform::Event::Type::TouchEnded, {130.0F, 200.0F}));
    EXPECT_EQ(lua("return input.touches()[1].phase"), "ended");
    fixture.frames(1);
    EXPECT_EQ(lua("return #input.touches()"), "0");
}

TEST_F(InputLuaTest, ReadsGamepads) {
    GamepadState pad{.connected = true, .name = "Test Pad"};
    pad.buttons[static_cast<std::size_t>(GamepadButton::South)] = true;
    pad.axes[static_cast<std::size_t>(GamepadAxis::LeftX)] = 1.0F;
    pad.axes[static_cast<std::size_t>(GamepadAxis::RightTrigger)] = 0.05F;
    fixture.host().setGamepad(1, pad);
    fixture.frames(1);

    EXPECT_EQ(lua("return input.gamepadConnected(2) and not input.gamepadConnected()"), "true");
    EXPECT_EQ(lua("return input.gamepadName(2)"), "Test Pad");
    EXPECT_EQ(lua("return input.gamepadDown('south', 2) and input.gamepadPressed('south', 2)"), "true");
    EXPECT_EQ(lua("return input.gamepadAxis('left_x', 2) .. ' ' .. input.gamepadAxis('right_trigger', 2)"), "1.0 0.0");
    EXPECT_EQ(lua("local x, y = input.gamepadStick('left', 2) return x .. ',' .. y"), "1.0,0.0");
    EXPECT_EQ(lua("return input.lastDevice()"), "gamepad");

    lua("input.setGamepadDeadzone(0)");
    fixture.frames(1);
    EXPECT_EQ(lua("return input.gamepadAxis('right_trigger', 2) > 0"), "true");

    pad.buttons.fill(false);
    fixture.host().setGamepad(1, pad);
    fixture.frames(1);
    EXPECT_EQ(lua("return input.gamepadReleased('south', 2)"), "true");
    EXPECT_NE(lua("return input.gamepadDown('south', 9)").find("gamepad index out of range"), std::string::npos);
    EXPECT_NE(lua("return input.gamepadStick('middle')").find("expected left or right"), std::string::npos);
}

TEST_F(InputLuaTest, MapsActionsFromEveryDevice) {
    // clang-format off
    lua(R"(
        input.loadActions({actions = {
            {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
            {name = 'throttle', type = 'axis', positive = {'key:w'}, negative = {'key:s'}},
            {name = 'move', type = 'vector', up = {'key:up'}, down = {'key:down'}, left = {'key:left'}, right = {'key:right'}, bindings = {'virtual_stick:move'}},
        }})
    )");
    // clang-format on
    EXPECT_EQ(lua("return table.concat(input.actionNames(), ',')"), "jump,throttle,move");

    core::Engine& engine = fixture.engine();
    engine.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::W));
    engine.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::Right));
    fixture.frames(1);
    EXPECT_EQ(lua("return input.value('throttle')"), "1.0");
    EXPECT_EQ(lua("local x, y = input.vector('move') return x .. ',' .. y"), "1.0,0.0");
    EXPECT_EQ(lua("return input.down('jump')"), "false");

    lua("input.setVirtualButton('jump', true)");
    fixture.frames(1);
    EXPECT_EQ(lua("return input.down('jump') and input.pressed('jump')"), "true");
    lua("input.setVirtualButton('jump', false)");
    fixture.frames(1);
    EXPECT_EQ(lua("return input.released('jump')"), "true");

    engine.handleEvent(makeKeyEvent(platform::Event::Type::KeyUp, Key::Right));
    lua("input.setVirtualStick('move', 0, -1)");
    fixture.frames(1);
    EXPECT_EQ(lua("local x, y = input.vector('move') return x .. ',' .. y"), "0.0,-1.0");
    lua("input.clearVirtual()");
    fixture.frames(1);
    EXPECT_EQ(lua("local x, y = input.vector('move') return x .. ',' .. y"), "0.0,0.0");

    EXPECT_EQ(lua("return input.saveActions().actions[1].bindings[2]"), "button:south");
    EXPECT_EQ(lua("input.setGamepadIndex(2) input.setGamepadIndex(nil) return 'ok'"), "ok");
    EXPECT_EQ(lua("return input.down('unknown')"), "false");
    EXPECT_NE(lua("input.loadActions({actions = {{name = 'x', type = 'button', bindings = {'key:nope'}}}})").find("Invalid input binding: key:nope"), std::string::npos);
    EXPECT_NE(lua("input.loadActions({actions = {{name = 'x', type = 'trigger'}}})").find("Invalid action type: trigger"), std::string::npos);
}

TEST(InputLuaAssetsTest, LoadsActionsFromAssets) {
    test::EngineFixture fixture({{"content/input.json", R"({"actions": [{"name": "fire", "type": "button", "bindings": ["mouse:left"]}]})"}});
    EXPECT_EQ(fixture.lua("local input = require('haylen.input') input.loadActions('input.json') return input.actionNames()[1]"), "fire");
}

TEST_F(InputLuaTest, DeliversEventsToScenesInDesignCoordinates) {
    fixture.host().resize({960.0F, 540.0F});
    fixture.frames(1);
    lua("events = {} require('haylen.scene').push({event = function(self, e) events[#events + 1] = e end})");
    fixture.frames(1);

    core::Engine& engine = fixture.engine();
    engine.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::A));
    engine.handleEvent({.type = platform::Event::Type::Character, .character = U'a'});
    engine.handleEvent(makeMouseEvent(platform::Event::Type::MouseDown, {10.0F, 20.0F}));
    engine.handleEvent({.type = platform::Event::Type::MouseScroll, .scroll = {1.0F, 0.0F}});
    engine.handleEvent(makeTouchEvent(platform::Event::Type::TouchBegan, {5.0F, 5.0F}));
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    engine.handleEvent({.type = platform::Event::Type::MouseMove, .modifiers = {.control = true, .alt = true}, .position = {30.0F, 40.0F}, .delta = {3.0F, -4.0F}});

    EXPECT_EQ(lua("return events[1].type .. ' ' .. events[1].key .. ' ' .. tostring(events[1]['repeat'])"), "key_down a false");
    EXPECT_EQ(lua("local m = events[1].modifiers return tostring(m.shift) .. tostring(m.control) .. tostring(m.alt) .. tostring(m.super)"), "truefalsefalsefalse");
    EXPECT_EQ(lua("return events[2].character"), "a");
    EXPECT_EQ(lua("return events[3].type .. ' ' .. events[3].button .. ' ' .. events[3].x .. ',' .. events[3].y"), "mouse_down left 20.0,40.0");
    EXPECT_EQ(lua("return events[4].scrollX"), "1.0");
    EXPECT_EQ(lua("local t = events[5].touches[1] return t.id .. ' ' .. t.x .. ' ' .. tostring(t.changed)"), "7 10.0 true");
    EXPECT_EQ(lua("return events[6].type .. ' ' .. tostring(events[6].modifiers)"), "focus_lost nil");
    EXPECT_EQ(lua("local e = events[7] return e.x .. ',' .. e.y .. ' ' .. e.dx .. ',' .. e.dy .. ' ' .. tostring(e.button) .. ' ' .. tostring(e.modifiers.control and e.modifiers.alt)"), "60.0,80.0 6.0,-8.0 nil true");
}

TEST_F(InputLuaTest, ReadsSingleTouchesAndDeviceSettings) {
    core::Engine& engine = fixture.engine();
    fixture.host().resize({960.0F, 540.0F});
    fixture.frames(1);
    engine.handleEvent(makeTouchEvent(platform::Event::Type::TouchBegan, {100.0F, 200.0F}));
    engine.handleEvent(makeMouseEvent(platform::Event::Type::MouseMove, {30.0F, 15.0F}));
    EXPECT_EQ(lua("local t = input.touch(7) return t.x .. ',' .. t.y .. ' ' .. t.phase .. ' ' .. tostring(input.touch(8))"), "200.0,400.0 began nil");
    EXPECT_EQ(lua("local x, y = input.mouseFramebufferPosition() return x .. ',' .. y"), "30.0,15.0");

    EXPECT_EQ(lua("return input.gamepadDeadzone() > 0.19 and input.gamepadDeadzone() < 0.21"), "true");
    EXPECT_EQ(lua("input.setGamepadDeadzone(0.5) return input.gamepadDeadzone()"), "0.5");

    EXPECT_EQ(lua("local s = input.gestureSettings() return s.longPressDuration .. ' ' .. s.swipeMinDistance .. ' ' .. tostring(s.mouse)"), "0.5 90.0 true");
    lua("input.setGestureSettings({longPressDuration = 0.75, mouse = false})");
    EXPECT_EQ(lua("local s = input.gestureSettings() return s.longPressDuration .. ' ' .. s.tapMaxMovement .. ' ' .. tostring(s.mouse)"), "0.75 24.0 false");
}

TEST_F(InputLuaTest, DefinesActionsOneByOne) {
    lua("input.defineAction({name = 'jump', type = 'button', bindings = {'key:space'}})");
    lua("input.defineAction({name = 'move', type = 'vector', left = {'key:a'}, right = {'key:d'}, bindings = {}})");
    EXPECT_EQ(lua("return table.concat(input.actionNames(), ',')"), "jump,move");
    EXPECT_EQ(lua("local d = input.actionDefinition('move') return d.type .. ' ' .. d.left[1] .. ' ' .. tostring(d.bindings)"), "vector key:a nil");
    EXPECT_EQ(lua("return tostring(input.actionDefinition('fly'))"), "nil");

    lua("input.defineAction({name = 'jump', type = 'button', bindings = {'key:w'}})");
    EXPECT_EQ(lua("return table.concat(input.actionNames(), ',') .. ' ' .. input.actionDefinition('jump').bindings[1]"), "jump,move key:w");

    core::Engine& engine = fixture.engine();
    engine.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::W));
    fixture.frames(1);
    EXPECT_EQ(lua("return input.down('jump')"), "true");

    lua("input.removeAction('jump')");
    EXPECT_EQ(lua("return table.concat(input.actionNames(), ',')"), "move");
    lua("input.clearActions()");
    EXPECT_EQ(lua("return #input.actionNames()"), "0");

    EXPECT_NE(lua("input.defineAction({name = 'x', type = 'button', bindings = {42}})").find("Invalid input binding: 42"), std::string::npos);
    EXPECT_NE(lua("input.defineAction({type = 'button'})").find("An action needs a name and a type."), std::string::npos);
    EXPECT_NE(lua("input.defineAction({name = 'x', type = 'button', bindings = 'key:a'})").find("The bindings of an action must be a list of bindings."), std::string::npos);
    EXPECT_NE(lua("input.defineAction({name = 'x', type = 'button', hold = true})").find("Unknown key 'hold' in an action."), std::string::npos);
}

} // namespace haylen::input
