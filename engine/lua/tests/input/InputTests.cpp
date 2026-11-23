#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <stdexcept>
#include <string>

#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/platform/Event.hpp"
#include "input/InputFixture.hpp"
#include "support/AllocationTracker.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::input {

class InputTest : public haylen::input::InputFixture {};

TEST(ControlsTest, NamesRoundTrip) {
    EXPECT_EQ(haylen::input::Controls::keyFromName("leftShift"), haylen::input::Key::LeftShift);
    EXPECT_EQ(haylen::input::Controls::keyName(haylen::input::Key::F12), "f12");
    EXPECT_EQ(haylen::input::Controls::keyFromName("digit1"), haylen::input::Key::Digit1);
    EXPECT_EQ(haylen::input::Controls::keyName(haylen::input::Key::Digit0), "digit0");
    EXPECT_EQ(haylen::input::Controls::keyName(haylen::input::Key::Unknown), "unknown");
    EXPECT_FALSE(haylen::input::Controls::keyFromName("hyper").has_value());
    EXPECT_EQ(haylen::input::Controls::mouseButtonFromName("middle"), haylen::input::MouseButton::Middle);
    EXPECT_EQ(haylen::input::Controls::mouseButtonName(haylen::input::MouseButton::Right), "right");
    EXPECT_FALSE(haylen::input::Controls::mouseButtonFromName("extra").has_value());
    EXPECT_EQ(haylen::input::Controls::gamepadButtonFromName("dpadLeft"), haylen::input::GamepadButton::DpadLeft);
    EXPECT_EQ(haylen::input::Controls::gamepadButtonName(haylen::input::GamepadButton::Start), "start");
    EXPECT_EQ(haylen::input::Controls::gamepadAxisFromName("rightTrigger"), haylen::input::GamepadAxis::RightTrigger);
    EXPECT_EQ(haylen::input::Controls::gamepadAxisName(haylen::input::GamepadAxis::LeftY), "leftY");
}

TEST_F(InputTest, KeyboardEdgesSurviveATapWithinOneFrame) {
    haylen::input::Input input;
    const haylen::graphics::Viewport viewport = makeIdentityViewport();

    input.handleEvent(makeKeyEvent(haylen::platform::Event::Type::KeyDown, haylen::input::Key::Space), viewport);
    input.handleEvent(makeKeyEvent(haylen::platform::Event::Type::KeyUp, haylen::input::Key::Space), viewport);
    EXPECT_TRUE(input.isKeyPressed(haylen::input::Key::Space));
    EXPECT_TRUE(input.isKeyReleased(haylen::input::Key::Space));
    EXPECT_FALSE(input.isKeyDown(haylen::input::Key::Space));
    EXPECT_TRUE(input.isAnyKeyPressed());

    input.endFrame();
    EXPECT_FALSE(input.isKeyPressed(haylen::input::Key::Space));
    EXPECT_FALSE(input.isAnyKeyPressed());

    input.handleEvent(makeKeyEvent(haylen::platform::Event::Type::KeyDown, haylen::input::Key::A), viewport);
    input.endFrame();
    input.handleEvent(makeKeyEvent(haylen::platform::Event::Type::KeyDown, haylen::input::Key::A, true), viewport);
    EXPECT_TRUE(input.isKeyDown(haylen::input::Key::A));
    EXPECT_FALSE(input.isKeyPressed(haylen::input::Key::A));
    EXPECT_FALSE(input.isKeyDown(static_cast<haylen::input::Key>(9999)));
    EXPECT_FALSE(input.isKeyPressed(static_cast<haylen::input::Key>(9999)));
    EXPECT_FALSE(input.isKeyReleased(static_cast<haylen::input::Key>(9999)));
}

TEST_F(InputTest, TextModifiersAndFocusLoss) {
    haylen::input::Input input;
    const haylen::graphics::Viewport viewport = makeIdentityViewport();

    haylen::platform::Event character;
    character.type = haylen::platform::Event::Type::Character;
    character.character = U'é';
    input.handleEvent(character, viewport);
    ASSERT_EQ(input.getText().size(), 1U);
    EXPECT_EQ(input.getText()[0], U'é');

    haylen::platform::Event shift = makeKeyEvent(haylen::platform::Event::Type::KeyDown, haylen::input::Key::LeftShift);
    shift.modifiers = {.shift = true};
    input.handleEvent(shift, viewport);
    EXPECT_EQ(input.getModifiers(), (haylen::input::KeyModifiers{.shift = true}));

    input.handleEvent(makeMouseEvent(haylen::platform::Event::Type::MouseDown, haylen::input::MouseButton::Left), viewport);
    input.releaseAll();
    EXPECT_FALSE(input.isKeyDown(haylen::input::Key::LeftShift));
    EXPECT_TRUE(input.isKeyReleased(haylen::input::Key::LeftShift));
    EXPECT_EQ(input.getModifiers(), haylen::input::KeyModifiers{});
    EXPECT_FALSE(input.isMouseDown(haylen::input::MouseButton::Left));
    EXPECT_TRUE(input.isMouseReleased(haylen::input::MouseButton::Left));

    input.endFrame();
    EXPECT_TRUE(input.getText().empty());
}

TEST_F(InputTest, MouseIsReportedInDesignCoordinates) {
    haylen::input::Input input;
    haylen::graphics::Viewport viewport;
    viewport.update(haylen::math::Vec2(960.0F, 540.0F), haylen::math::Vec2(1920.0F, 1080.0F), haylen::graphics::Viewport::ScalingPolicy::Fit);

    haylen::platform::Event move;
    move.type = haylen::platform::Event::Type::MouseMove;
    move.position = haylen::math::Vec2(480.0F, 270.0F);
    input.handleEvent(move, viewport);
    EXPECT_EQ(input.getMousePosition(), haylen::math::Vec2(960.0F, 540.0F));
    EXPECT_EQ(input.getMouseFramebufferPosition(), haylen::math::Vec2(480.0F, 270.0F));
    EXPECT_EQ(input.getMouseDelta(), haylen::math::Vec2{}) << "The pointer arrives without moving.";
    EXPECT_TRUE(input.isMouseInside());

    move.position = haylen::math::Vec2(485.0F, 270.0F);
    input.handleEvent(move, viewport);
    EXPECT_EQ(input.getMouseDelta(), haylen::math::Vec2(10.0F, 0.0F));
    move.position = haylen::math::Vec2(490.0F, 270.0F);
    move.delta = haylen::math::Vec2(5.0F, 0.0F);
    input.handleEvent(move, viewport);
    EXPECT_EQ(input.getMouseDelta(), haylen::math::Vec2(20.0F, 0.0F));

    haylen::platform::Event scroll;
    scroll.type = haylen::platform::Event::Type::MouseScroll;
    scroll.scroll = haylen::math::Vec2(0.0F, 2.0F);
    input.handleEvent(scroll, viewport);
    EXPECT_EQ(input.getMouseScroll(), haylen::math::Vec2(0.0F, 2.0F));

    // A trackpad counts every forty points it moves as a step of a wheel.
    scroll.scroll = haylen::math::Vec2(-20.0F, 120.0F);
    scroll.preciseScroll = true;
    input.handleEvent(scroll, viewport);
    EXPECT_EQ(input.getMouseScroll(), haylen::math::Vec2(-0.5F, 5.0F));
    EXPECT_THROW(input.setScrollSpeed(0.0F), std::invalid_argument);
    EXPECT_THROW(input.setScrollSpeed(std::numeric_limits<float>::infinity()), std::invalid_argument);
    input.setScrollSpeed(2.0F);
    EXPECT_EQ(input.getScrollSpeed(), 2.0F);

    input.handleEvent(makeMouseEvent(haylen::platform::Event::Type::MouseDown, haylen::input::MouseButton::Right), viewport);
    EXPECT_TRUE(input.isMousePressed(haylen::input::MouseButton::Right));
    EXPECT_TRUE(input.isMouseDown(haylen::input::MouseButton::Right));
    input.handleEvent(makeMouseEvent(haylen::platform::Event::Type::MouseUp, haylen::input::MouseButton::Right), viewport);
    EXPECT_TRUE(input.isMouseReleased(haylen::input::MouseButton::Right));

    haylen::platform::Event leave;
    leave.type = haylen::platform::Event::Type::MouseLeave;
    input.handleEvent(leave, viewport);
    EXPECT_FALSE(input.isMouseInside());
    leave.type = haylen::platform::Event::Type::MouseEnter;
    input.handleEvent(leave, viewport);
    EXPECT_TRUE(input.isMouseInside());

    input.endFrame();
    EXPECT_EQ(input.getMouseDelta(), haylen::math::Vec2{});
    EXPECT_EQ(input.getMouseScroll(), haylen::math::Vec2{});
    EXPECT_EQ(input.getLastDevice(), haylen::input::InputDevice::KeyboardMouse);
}

TEST_F(InputTest, TouchesMoveThroughPhases) {
    haylen::input::Input input;
    const haylen::graphics::Viewport viewport = makeIdentityViewport();

    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchBegan, 1, haylen::math::Vec2(10.0F, 10.0F)), viewport);
    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchBegan, 1, haylen::math::Vec2(10.0F, 10.0F)), viewport);
    ASSERT_EQ(input.getTouches().size(), 1U);
    EXPECT_EQ(input.getTouches()[0].phase, haylen::input::TouchPhase::Began);
    EXPECT_EQ(input.getLastDevice(), haylen::input::InputDevice::Touch);

    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchMoved, 1, haylen::math::Vec2(20.0F, 10.0F)), viewport);
    EXPECT_EQ(touchOf(input, 1).phase, haylen::input::TouchPhase::Began);
    input.updateTouchDurations(0.5F);
    input.endFrame();
    EXPECT_EQ(touchOf(input, 1).phase, haylen::input::TouchPhase::Stationary);
    EXPECT_FLOAT_EQ(touchOf(input, 1).duration, 0.5F);

    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchMoved, 1, haylen::math::Vec2(30.0F, 10.0F)), viewport);
    EXPECT_EQ(touchOf(input, 1).phase, haylen::input::TouchPhase::Moved);
    EXPECT_EQ(touchOf(input, 1).previousPosition, haylen::math::Vec2(20.0F, 10.0F));
    EXPECT_EQ(touchOf(input, 1).startPosition, haylen::math::Vec2(10.0F, 10.0F));

    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchMoved, 5, haylen::math::Vec2(0.0F, 0.0F)), viewport);
    EXPECT_EQ(input.findTouch(5), nullptr);

    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchEnded, 1, haylen::math::Vec2(30.0F, 10.0F)), viewport);
    EXPECT_EQ(touchOf(input, 1).phase, haylen::input::TouchPhase::Ended);
    input.endFrame();
    EXPECT_TRUE(input.getTouches().empty());

    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchBegan, 2, haylen::math::Vec2{}), viewport);
    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchCancelled, 2, haylen::math::Vec2{}), viewport);
    EXPECT_EQ(touchOf(input, 2).phase, haylen::input::TouchPhase::Cancelled);
}

TEST_F(InputTest, AReusedTouchIdStartsANewTouch) {
    haylen::input::Input input;
    const haylen::graphics::Viewport viewport = makeIdentityViewport();
    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchBegan, 1, haylen::math::Vec2(10.0F, 10.0F)), viewport);
    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchEnded, 1, haylen::math::Vec2(10.0F, 10.0F)), viewport);
    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchBegan, 1, haylen::math::Vec2(50.0F, 50.0F)), viewport);
    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchMoved, 1, haylen::math::Vec2(60.0F, 50.0F)), viewport);

    ASSERT_EQ(input.getTouches().size(), 2U);
    EXPECT_EQ(input.getTouches()[0].phase, haylen::input::TouchPhase::Ended);
    EXPECT_EQ(input.getTouches()[0].position, haylen::math::Vec2(10.0F, 10.0F));
    EXPECT_EQ(input.getTouches()[1].phase, haylen::input::TouchPhase::Began);
    EXPECT_EQ(input.getTouches()[1].startPosition, haylen::math::Vec2(50.0F, 50.0F));
    EXPECT_EQ(input.getTouches()[1].position, haylen::math::Vec2(60.0F, 50.0F));

    input.endFrame();
    ASSERT_EQ(input.getTouches().size(), 1U);
    EXPECT_EQ(touchOf(input, 1).phase, haylen::input::TouchPhase::Stationary);
}

TEST_F(InputTest, TouchMovesOfOneFrameAddUp) {
    haylen::input::Input input;
    const haylen::graphics::Viewport viewport = makeIdentityViewport();
    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchBegan, 1, haylen::math::Vec2(10.0F, 10.0F)), viewport);
    input.endFrame();

    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchMoved, 1, haylen::math::Vec2(20.0F, 10.0F)), viewport);
    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchMoved, 1, haylen::math::Vec2(35.0F, 15.0F)), viewport);
    const haylen::input::Touch& touch = touchOf(input, 1);
    EXPECT_EQ(touch.position - touch.previousPosition, haylen::math::Vec2(25.0F, 5.0F));
}

TEST_F(InputTest, GamepadEdgesAxesAndDeadzones) {
    haylen::input::Input input;
    std::array<haylen::input::GamepadState, 1> states{makeGamepad(haylen::input::GamepadButton::South, 0.1F)};
    input.updateGamepads(states);
    EXPECT_TRUE(input.isGamepadPressed(0, haylen::input::GamepadButton::South));
    EXPECT_TRUE(input.isGamepadDown(0, haylen::input::GamepadButton::South));
    EXPECT_EQ(input.getGamepadAxis(0, haylen::input::GamepadAxis::LeftX), 0.0F);
    EXPECT_EQ(input.getLastDevice(), haylen::input::InputDevice::Gamepad);
    EXPECT_TRUE(input.getGamepad(0).connected);
    EXPECT_FALSE(input.getGamepad(1).connected);

    states[0] = makeGamepad(haylen::input::GamepadButton::North, 0.6F);
    input.updateGamepads(states);
    EXPECT_TRUE(input.isGamepadReleased(0, haylen::input::GamepadButton::South));
    EXPECT_FALSE(input.isGamepadPressed(0, haylen::input::GamepadButton::South));
    EXPECT_NEAR(input.getGamepadAxis(0, haylen::input::GamepadAxis::LeftX), 0.5F, 1e-5F);
    EXPECT_NEAR(input.getGamepadStick(0, false).x, 0.5F, 1e-5F);
    EXPECT_EQ(input.getGamepadStick(0, true), haylen::math::Vec2{});

    input.setGamepadDeadzone(0.7F);
    EXPECT_EQ(input.getGamepadDeadzone(), 0.7F);
    EXPECT_EQ(input.getGamepadStick(0, false), haylen::math::Vec2{});

    // A negative dead zone would move a resting stick, and one of 1 or more would silence every axis.
    EXPECT_THROW(input.setGamepadDeadzone(-0.1F), std::invalid_argument);
    EXPECT_THROW(input.setGamepadDeadzone(1.0F), std::invalid_argument);
    EXPECT_THROW(input.setGamepadDeadzone(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
    EXPECT_EQ(input.getGamepadDeadzone(), 0.7F);

    EXPECT_FALSE(input.getGamepad(7).connected);
    EXPECT_FALSE(input.isGamepadDown(7, haylen::input::GamepadButton::South));
    EXPECT_FALSE(input.isGamepadPressed(7, haylen::input::GamepadButton::South));
    EXPECT_FALSE(input.isGamepadReleased(7, haylen::input::GamepadButton::South));
    EXPECT_EQ(input.getGamepadAxis(7, haylen::input::GamepadAxis::LeftX), 0.0F);
    EXPECT_EQ(input.getGamepadStick(7, false), haylen::math::Vec2{});
}

TEST_F(InputTest, LastDeviceFollowsPressesInsteadOfHeldControls) {
    haylen::input::Input input;
    const haylen::graphics::Viewport viewport = makeIdentityViewport();
    std::array<haylen::input::GamepadState, 1> states{makeGamepad(haylen::input::GamepadButton::South)};
    input.updateGamepads(states);
    EXPECT_EQ(input.getLastDevice(), haylen::input::InputDevice::Gamepad);

    // A key pressed while the gamepad button stays down takes over, and so does a touch while a stick stays pushed.
    input.handleEvent(makeKeyEvent(haylen::platform::Event::Type::KeyDown, haylen::input::Key::A), viewport);
    input.updateGamepads(states);
    EXPECT_EQ(input.getLastDevice(), haylen::input::InputDevice::KeyboardMouse);

    states[0].axes[static_cast<std::size_t>(haylen::input::GamepadAxis::LeftX)] = 0.5F;
    input.updateGamepads(states);
    EXPECT_EQ(input.getLastDevice(), haylen::input::InputDevice::Gamepad);

    input.handleEvent(makeKeyEvent(haylen::platform::Event::Type::KeyDown, haylen::input::Key::A, true), viewport);
    EXPECT_EQ(input.getLastDevice(), haylen::input::InputDevice::Gamepad);

    // The on-screen keyboard types for the device in hand.
    haylen::platform::Event onScreen = makeKeyEvent(haylen::platform::Event::Type::KeyDown, haylen::input::Key::Backspace);
    onScreen.typedOnScreen = true;
    input.handleEvent(onScreen, viewport);
    EXPECT_EQ(input.getLastDevice(), haylen::input::InputDevice::Gamepad);

    input.handleEvent(makeTouchEvent(haylen::platform::Event::Type::TouchBegan, 1, haylen::math::Vec2(10.0F, 10.0F)), viewport);
    input.updateGamepads(states);
    EXPECT_EQ(input.getLastDevice(), haylen::input::InputDevice::Touch);
}

TEST(VirtualInputTest, StoresButtonsAndClampedSticks) {
    haylen::input::VirtualInput controls;
    controls.setButton("attack", true);
    controls.setStick("move", haylen::math::Vec2(3.0F, 4.0F));
    EXPECT_TRUE(controls.isButtonDown("attack"));
    EXPECT_FALSE(controls.isButtonDown("jump"));
    EXPECT_NEAR(controls.getStick("move").getLength(), 1.0F, 1e-5F);
    EXPECT_EQ(controls.getStick("look"), haylen::math::Vec2{});
    controls.clear();
    EXPECT_FALSE(controls.isButtonDown("attack"));
}

TEST(VirtualInputTest, KeepsAHeldControlWithoutAllocating) {
    haylen::input::VirtualInput controls;
    controls.setButton("special attack of the left hand", true);
    controls.setStick("movement of the left thumb stick", haylen::math::Vec2(0.5F, 0.0F));
    const test::AllocationTracker tracker;
    controls.setButton("special attack of the left hand", true);
    controls.setStick("movement of the left thumb stick", haylen::math::Vec2(0.25F, 0.0F));
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(controls.getStick("movement of the left thumb stick"), haylen::math::Vec2(0.25F, 0.0F));
}

TEST(BindingTest, ParsesAndPrintsEveryForm) {
    for (const std::string text : {"key:w", "mouse:left", "button:south", "axis:leftY-", "axis:rightTrigger+", "stick:right", "virtual:attack", "virtualStick:move"}) {
        const auto binding = haylen::input::ActionMap::Binding::parse(text);
        ASSERT_TRUE(binding.has_value()) << text;
        EXPECT_EQ(binding->toString(), text);
    }

    for (const std::string text : {"w", "key:hyper", "mouse:extra", "button:x", "axis:leftY", "axis:bad+", "stick:middle", "virtual:", "device:foo"}) {
        EXPECT_FALSE(haylen::input::ActionMap::Binding::parse(text).has_value()) << text;
    }
}

TEST(InputLuaBindingTest, ReportsThePointerCapture) {
    test::LuaFixture fixture;
    fixture.runLua("input = require('haylen.input')");
    EXPECT_EQ(fixture.lua("return input.pointerCaptured()"), "false");
    fixture.engine().getInput().setPointerCaptured(true);
    EXPECT_EQ(fixture.lua("return input.pointerCaptured()"), "true");
}

TEST(InputLuaBindingTest, RejectsDeadZonesAndThresholdsOutOfRange) {
    test::LuaFixture fixture;
    fixture.runLua("input = require('haylen.input')");
    EXPECT_NE(fixture.lua("input.setGamepadDeadzone(1)").find("The gamepad dead zone must be at least 0 and below 1."), std::string::npos);
    EXPECT_NE(fixture.lua("input.setGamepadDeadzone(-0.5)").find("The gamepad dead zone must be at least 0 and below 1."), std::string::npos);
    EXPECT_NE(fixture.lua("input.setPressThreshold(0)").find("The press threshold must be above 0 and at most 1."), std::string::npos);
    EXPECT_NE(fixture.lua("input.setPressThreshold(2)").find("The press threshold must be above 0 and at most 1."), std::string::npos);
    EXPECT_EQ(fixture.lua("input.setGamepadDeadzone(0) input.setPressThreshold(1) return input.gamepadDeadzone()"), "0.0");
}

} // namespace haylen::lua::input
