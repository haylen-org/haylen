#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>

#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::input {

// Builds the platform events, viewports and gamepad states that drive the input tests.
class InputEventTest : public ::testing::Test {
  protected:
    [[nodiscard]] static platform::Event makeKeyEvent(platform::Event::Type type, Key key, bool repeat = false) {
        platform::Event event;
        event.type = type;
        event.key = key;
        event.repeat = repeat;
        return event;
    }

    [[nodiscard]] static platform::Event makeMouseEvent(platform::Event::Type type, MouseButton button, math::Vec2 position = {}) {
        platform::Event event;
        event.type = type;
        event.mouseButton = button;
        event.position = position;
        return event;
    }

    [[nodiscard]] static platform::Event makeTouchEvent(platform::Event::Type type, std::uint64_t id, math::Vec2 position) {
        platform::Event event;
        event.type = type;
        event.touches[0] = platform::TouchPoint{id, position, true};
        event.touches[1] = platform::TouchPoint{id + 100, position, false};
        event.touchCount = 2;
        return event;
    }

    [[nodiscard]] static graphics::Viewport makeIdentityViewport() {
        graphics::Viewport viewport;
        viewport.update(math::Vec2(1920.0F, 1080.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::Fit);
        return viewport;
    }

    [[nodiscard]] static GamepadState makeGamepad(GamepadButton button, float leftX = 0.0F) {
        GamepadState state;
        state.connected = true;
        state.buttons[static_cast<std::size_t>(button)] = true;
        state.axes[static_cast<std::size_t>(GamepadAxis::LeftX)] = leftX;
        return state;
    }
};

class InputTest : public InputEventTest {};

class ActionMapTest : public InputEventTest {};

TEST(ControlsTest, NamesRoundTrip) {
    EXPECT_EQ(Controls::keyFromName("left_shift"), Key::LeftShift);
    EXPECT_EQ(Controls::keyName(Key::F12), "f12");
    EXPECT_EQ(Controls::keyName(Key::Unknown), "unknown");
    EXPECT_FALSE(Controls::keyFromName("hyper").has_value());
    EXPECT_EQ(Controls::mouseButtonFromName("middle"), MouseButton::Middle);
    EXPECT_EQ(Controls::mouseButtonName(MouseButton::Right), "right");
    EXPECT_FALSE(Controls::mouseButtonFromName("extra").has_value());
    EXPECT_EQ(Controls::gamepadButtonFromName("dpad_left"), GamepadButton::DpadLeft);
    EXPECT_EQ(Controls::gamepadButtonName(GamepadButton::Start), "start");
    EXPECT_EQ(Controls::gamepadAxisFromName("right_trigger"), GamepadAxis::RightTrigger);
    EXPECT_EQ(Controls::gamepadAxisName(GamepadAxis::LeftY), "left_y");
}

TEST_F(InputTest, KeyboardEdgesSurviveATapWithinOneFrame) {
    Input input;
    const graphics::Viewport viewport = makeIdentityViewport();

    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::Space), viewport);
    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyUp, Key::Space), viewport);
    EXPECT_TRUE(input.isKeyPressed(Key::Space));
    EXPECT_TRUE(input.isKeyReleased(Key::Space));
    EXPECT_FALSE(input.isKeyDown(Key::Space));
    EXPECT_TRUE(input.isAnyKeyPressed());

    input.endFrame();
    EXPECT_FALSE(input.isKeyPressed(Key::Space));
    EXPECT_FALSE(input.isAnyKeyPressed());

    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::A), viewport);
    input.endFrame();
    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::A, true), viewport);
    EXPECT_TRUE(input.isKeyDown(Key::A));
    EXPECT_FALSE(input.isKeyPressed(Key::A));
    EXPECT_FALSE(input.isKeyDown(static_cast<Key>(9999)));
    EXPECT_FALSE(input.isKeyPressed(static_cast<Key>(9999)));
    EXPECT_FALSE(input.isKeyReleased(static_cast<Key>(9999)));
}

TEST_F(InputTest, TextModifiersAndFocusLoss) {
    Input input;
    const graphics::Viewport viewport = makeIdentityViewport();

    platform::Event character;
    character.type = platform::Event::Type::Character;
    character.character = U'é';
    input.handleEvent(character, viewport);
    ASSERT_EQ(input.getText().size(), 1U);
    EXPECT_EQ(input.getText()[0], U'é');

    platform::Event shift = makeKeyEvent(platform::Event::Type::KeyDown, Key::LeftShift);
    shift.modifiers = {.shift = true};
    input.handleEvent(shift, viewport);
    EXPECT_EQ(input.getModifiers(), (KeyModifiers{.shift = true}));

    input.handleEvent(makeMouseEvent(platform::Event::Type::MouseDown, MouseButton::Left), viewport);
    input.releaseAll();
    EXPECT_FALSE(input.isKeyDown(Key::LeftShift));
    EXPECT_TRUE(input.isKeyReleased(Key::LeftShift));
    EXPECT_FALSE(input.isMouseDown(MouseButton::Left));
    EXPECT_TRUE(input.isMouseReleased(MouseButton::Left));

    input.endFrame();
    EXPECT_TRUE(input.getText().empty());
}

TEST_F(InputTest, MouseIsReportedInDesignCoordinates) {
    Input input;
    graphics::Viewport viewport;
    viewport.update(math::Vec2(960.0F, 540.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::Fit);

    platform::Event move;
    move.type = platform::Event::Type::MouseMove;
    move.position = math::Vec2(480.0F, 270.0F);
    input.handleEvent(move, viewport);
    EXPECT_EQ(input.getMousePosition(), math::Vec2(960.0F, 540.0F));
    EXPECT_EQ(input.getMouseFramebufferPosition(), math::Vec2(480.0F, 270.0F));
    EXPECT_EQ(input.getMouseDelta(), math::Vec2(960.0F, 540.0F));
    EXPECT_TRUE(input.isMouseInside());

    move.position = math::Vec2(490.0F, 270.0F);
    move.delta = math::Vec2(5.0F, 0.0F);
    input.handleEvent(move, viewport);
    EXPECT_EQ(input.getMouseDelta(), math::Vec2(970.0F, 540.0F));

    platform::Event scroll;
    scroll.type = platform::Event::Type::MouseScroll;
    scroll.scroll = math::Vec2(0.0F, 2.0F);
    input.handleEvent(scroll, viewport);
    EXPECT_EQ(input.getMouseScroll(), math::Vec2(0.0F, 2.0F));

    input.handleEvent(makeMouseEvent(platform::Event::Type::MouseDown, MouseButton::Right), viewport);
    EXPECT_TRUE(input.isMousePressed(MouseButton::Right));
    EXPECT_TRUE(input.isMouseDown(MouseButton::Right));
    input.handleEvent(makeMouseEvent(platform::Event::Type::MouseUp, MouseButton::Right), viewport);
    EXPECT_TRUE(input.isMouseReleased(MouseButton::Right));

    platform::Event leave;
    leave.type = platform::Event::Type::MouseLeave;
    input.handleEvent(leave, viewport);
    EXPECT_FALSE(input.isMouseInside());
    leave.type = platform::Event::Type::MouseEnter;
    input.handleEvent(leave, viewport);
    EXPECT_TRUE(input.isMouseInside());

    input.endFrame();
    EXPECT_EQ(input.getMouseDelta(), math::Vec2{});
    EXPECT_EQ(input.getMouseScroll(), math::Vec2{});
    EXPECT_EQ(input.getLastDevice(), InputDevice::KeyboardMouse);
}

TEST_F(InputTest, TouchesMoveThroughPhases) {
    Input input;
    const graphics::Viewport viewport = makeIdentityViewport();

    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchBegan, 1, math::Vec2(10.0F, 10.0F)), viewport);
    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchBegan, 1, math::Vec2(10.0F, 10.0F)), viewport);
    ASSERT_EQ(input.getTouches().size(), 1U);
    EXPECT_EQ(input.getTouches()[0].phase, TouchPhase::Began);
    EXPECT_EQ(input.getLastDevice(), InputDevice::Touch);

    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchMoved, 1, math::Vec2(20.0F, 10.0F)), viewport);
    EXPECT_EQ(input.findTouch(1)->phase, TouchPhase::Began);
    input.updateTouchDurations(0.5F);
    input.endFrame();
    EXPECT_EQ(input.findTouch(1)->phase, TouchPhase::Stationary);
    EXPECT_FLOAT_EQ(input.findTouch(1)->duration, 0.5F);

    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchMoved, 1, math::Vec2(30.0F, 10.0F)), viewport);
    EXPECT_EQ(input.findTouch(1)->phase, TouchPhase::Moved);
    EXPECT_EQ(input.findTouch(1)->previousPosition, math::Vec2(20.0F, 10.0F));
    EXPECT_EQ(input.findTouch(1)->startPosition, math::Vec2(10.0F, 10.0F));

    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchMoved, 5, math::Vec2(0.0F, 0.0F)), viewport);
    EXPECT_EQ(input.findTouch(5), nullptr);

    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchEnded, 1, math::Vec2(30.0F, 10.0F)), viewport);
    EXPECT_EQ(input.findTouch(1)->phase, TouchPhase::Ended);
    input.endFrame();
    EXPECT_TRUE(input.getTouches().empty());

    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchBegan, 2, math::Vec2{}), viewport);
    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchCancelled, 2, math::Vec2{}), viewport);
    EXPECT_EQ(input.findTouch(2)->phase, TouchPhase::Cancelled);
}

TEST_F(InputTest, TouchMovesOfOneFrameAddUp) {
    Input input;
    const graphics::Viewport viewport = makeIdentityViewport();
    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchBegan, 1, math::Vec2(10.0F, 10.0F)), viewport);
    input.endFrame();

    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchMoved, 1, math::Vec2(20.0F, 10.0F)), viewport);
    input.handleEvent(makeTouchEvent(platform::Event::Type::TouchMoved, 1, math::Vec2(35.0F, 15.0F)), viewport);
    const Touch& touch = *input.findTouch(1);
    EXPECT_EQ(touch.position - touch.previousPosition, math::Vec2(25.0F, 5.0F));
}

TEST_F(InputTest, GamepadEdgesAxesAndDeadzones) {
    Input input;
    std::array<GamepadState, 1> states{makeGamepad(GamepadButton::South, 0.1F)};
    input.updateGamepads(states);
    EXPECT_TRUE(input.isGamepadPressed(0, GamepadButton::South));
    EXPECT_TRUE(input.isGamepadDown(0, GamepadButton::South));
    EXPECT_EQ(input.getGamepadAxis(0, GamepadAxis::LeftX), 0.0F);
    EXPECT_EQ(input.getLastDevice(), InputDevice::Gamepad);
    EXPECT_TRUE(input.getGamepad(0).connected);
    EXPECT_FALSE(input.getGamepad(1).connected);

    states[0] = makeGamepad(GamepadButton::North, 0.6F);
    input.updateGamepads(states);
    EXPECT_TRUE(input.isGamepadReleased(0, GamepadButton::South));
    EXPECT_FALSE(input.isGamepadPressed(0, GamepadButton::South));
    EXPECT_NEAR(input.getGamepadAxis(0, GamepadAxis::LeftX), 0.5F, 1e-5F);
    EXPECT_NEAR(input.getGamepadStick(0, false).x, 0.5F, 1e-5F);
    EXPECT_EQ(input.getGamepadStick(0, true), math::Vec2{});

    input.setGamepadDeadzone(0.7F);
    EXPECT_EQ(input.getGamepadDeadzone(), 0.7F);
    EXPECT_EQ(input.getGamepadStick(0, false), math::Vec2{});
    EXPECT_FALSE(input.isGamepadDown(7, GamepadButton::South));
    EXPECT_FALSE(input.isGamepadPressed(7, GamepadButton::South));
    EXPECT_FALSE(input.isGamepadReleased(7, GamepadButton::South));
    EXPECT_EQ(input.getGamepadAxis(7, GamepadAxis::LeftX), 0.0F);
    EXPECT_EQ(input.getGamepadStick(7, false), math::Vec2{});
}

TEST(VirtualInputTest, StoresButtonsAndClampedSticks) {
    VirtualInput controls;
    controls.setButton("attack", true);
    controls.setStick("move", math::Vec2(3.0F, 4.0F));
    EXPECT_TRUE(controls.isButtonDown("attack"));
    EXPECT_FALSE(controls.isButtonDown("jump"));
    EXPECT_NEAR(controls.getStick("move").getLength(), 1.0F, 1e-5F);
    EXPECT_EQ(controls.getStick("look"), math::Vec2{});
    controls.clear();
    EXPECT_FALSE(controls.isButtonDown("attack"));
}

TEST(BindingTest, ParsesAndPrintsEveryForm) {
    for (const std::string text : {"key:w", "mouse:left", "button:south", "axis:left_y-", "axis:right_trigger+", "stick:right", "virtual:attack", "virtual_stick:move"}) {
        const auto binding = ActionMap::Binding::parse(text);
        ASSERT_TRUE(binding.has_value()) << text;
        EXPECT_EQ(binding->toString(), text);
    }

    for (const std::string text : {"w", "key:hyper", "mouse:extra", "button:x", "axis:left_y", "axis:bad+", "stick:middle", "virtual:", "device:foo"}) {
        EXPECT_FALSE(ActionMap::Binding::parse(text).has_value()) << text;
    }
}

TEST_F(ActionMapTest, LoadsSavesAndReportsActions) {
    const core::Json document = core::Json::parse(R"({
        "actions": [
            {"name": "attack", "type": "button", "bindings": ["key:space", "mouse:left", "button:south", "virtual:attack"]},
            {"name": "zoom", "type": "axis", "positive": ["key:e", "axis:right_trigger+"], "negative": ["key:q"]},
            {"name": "move", "type": "vector", "up": ["key:w"], "down": ["key:s"], "left": ["key:a"], "right": ["key:d"], "bindings": ["stick:left", "virtual_stick:move"]}
        ]
    })");

    ActionMap actions;
    actions.load(document);
    EXPECT_EQ(actions.getNames(), (std::vector<std::string>{"attack", "zoom", "move"}));
    EXPECT_EQ(actions.save(), document);
    ASSERT_NE(actions.findAction("move"), nullptr);
    EXPECT_EQ(actions.findAction("move")->type, ActionMap::Action::Type::Vector);
    EXPECT_EQ(actions.findAction("missing"), nullptr);

    Input input;
    VirtualInput virtualInput;
    const graphics::Viewport viewport = makeIdentityViewport();

    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::D), viewport);
    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::W), viewport);
    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::E), viewport);
    input.handleEvent(makeMouseEvent(platform::Event::Type::MouseDown, MouseButton::Left), viewport);
    actions.update(input, virtualInput, false);

    EXPECT_TRUE(actions.isPressed("attack"));
    EXPECT_TRUE(actions.isDown("attack"));
    EXPECT_FLOAT_EQ(actions.getValue("zoom"), 1.0F);
    EXPECT_NEAR(actions.getVector("move").getLength(), 1.0F, 1e-5F);
    EXPECT_GT(actions.getVector("move").x, 0.0F);
    EXPECT_LT(actions.getVector("move").y, 0.0F);

    input.endFrame();
    input.handleEvent(makeMouseEvent(platform::Event::Type::MouseUp, MouseButton::Left), viewport);
    actions.update(input, virtualInput, false);
    EXPECT_FALSE(actions.isPressed("attack"));
    EXPECT_TRUE(actions.isReleased("attack"));

    EXPECT_FALSE(actions.isDown("missing"));
    EXPECT_FALSE(actions.isPressed("missing"));
    EXPECT_FALSE(actions.isReleased("missing"));
    EXPECT_EQ(actions.getValue("missing"), 0.0F);
    EXPECT_EQ(actions.getVector("missing"), math::Vec2{});
}

TEST_F(ActionMapTest, VirtualControlsAndGamepadsDriveActions) {
    ActionMap actions;
    actions.define({.name = "attack", .type = ActionMap::Action::Type::Button, .bindings = {*ActionMap::Binding::parse("virtual:attack"), *ActionMap::Binding::parse("button:east")}});
    actions.define({.name = "move", .type = ActionMap::Action::Type::Vector, .bindings = {*ActionMap::Binding::parse("virtual_stick:move"), *ActionMap::Binding::parse("stick:left")}});
    actions.define({.name = "aim", .type = ActionMap::Action::Type::Axis, .positive = {*ActionMap::Binding::parse("axis:left_x+")}});

    Input input;
    VirtualInput virtualInput;
    virtualInput.setButton("attack", true);
    virtualInput.setStick("move", math::Vec2(0.0F, -1.0F));
    actions.update(input, virtualInput, false);
    EXPECT_TRUE(actions.isDown("attack"));
    EXPECT_EQ(actions.getVector("move"), math::Vec2(0.0F, -1.0F));

    virtualInput.clear();
    std::array<GamepadState, 2> states{GamepadState{}, makeGamepad(GamepadButton::East, 1.0F)};
    states[0].connected = true;
    input.updateGamepads(states);
    actions.update(input, virtualInput, false);
    EXPECT_TRUE(actions.isDown("attack"));
    EXPECT_NEAR(actions.getVector("move").x, 1.0F, 1e-5F);
    EXPECT_NEAR(actions.getValue("aim"), 1.0F, 1e-5F);

    actions.setGamepadIndex(0);
    actions.update(input, virtualInput, false);
    EXPECT_FALSE(actions.isDown("attack"));
    EXPECT_EQ(actions.getVector("move"), math::Vec2{});
    EXPECT_EQ(actions.getValue("aim"), 0.0F);

    actions.setGamepadIndex(std::nullopt);
    actions.setPressThreshold(2.0F);
    actions.update(input, virtualInput, false);
    EXPECT_FALSE(actions.isDown("attack"));

    actions.define({.name = "attack", .type = ActionMap::Action::Type::Button});
    EXPECT_TRUE(actions.findAction("attack")->bindings.empty());
    EXPECT_EQ(actions.getNames().front(), "attack");
    actions.remove("attack");
    EXPECT_EQ(actions.findAction("attack"), nullptr);
    actions.clear();
    EXPECT_TRUE(actions.getNames().empty());
}

TEST_F(ActionMapTest, BlockedInputHoldsEveryControlUntilItIsReleased) {
    ActionMap actions;
    actions.define({.name = "key", .type = ActionMap::Action::Type::Button, .bindings = {*ActionMap::Binding::parse("key:p")}});
    actions.define({.name = "mouse", .type = ActionMap::Action::Type::Button, .bindings = {*ActionMap::Binding::parse("mouse:right")}});
    actions.define({.name = "button", .type = ActionMap::Action::Type::Button, .bindings = {*ActionMap::Binding::parse("button:start")}});
    actions.define({.name = "axis", .type = ActionMap::Action::Type::Axis, .positive = {*ActionMap::Binding::parse("axis:left_x+")}});
    actions.define({.name = "virtual", .type = ActionMap::Action::Type::Button, .bindings = {*ActionMap::Binding::parse("virtual:attack")}});
    actions.define({.name = "stick", .type = ActionMap::Action::Type::Vector, .bindings = {*ActionMap::Binding::parse("virtual_stick:move")}});
    const std::array<std::string, 6> names{"key", "mouse", "button", "axis", "virtual", "stick"};
    // clang-format off
    const auto read = [&actions, &names](bool (ActionMap::*query)(std::string_view) const noexcept) {
        std::string result;
        for (const std::string& name : names) {
            result += (actions.*query)(name) ? '1' : '0';
        }
        return result;
    };
    // clang-format on

    Input input;
    VirtualInput virtualInput;
    const graphics::Viewport viewport = makeIdentityViewport();
    std::array<GamepadState, 1> gamepads{makeGamepad(GamepadButton::Start, 1.0F)};
    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::P), viewport);
    actions.update(input, virtualInput, false);
    EXPECT_EQ(read(&ActionMap::isPressed), "100000");

    // Blocked input reads as idle, so the held key releases, and controls pressed meanwhile stay up.
    input.endFrame();
    input.handleEvent(makeMouseEvent(platform::Event::Type::MouseDown, MouseButton::Right), viewport);
    input.updateGamepads(gamepads);
    virtualInput.setButton("attack", true);
    virtualInput.setStick("move", math::Vec2(1.0F, 0.0F));
    actions.update(input, virtualInput, true);
    EXPECT_TRUE(actions.isReleased("key"));
    EXPECT_EQ(read(&ActionMap::isDown), "000000");
    EXPECT_EQ(actions.getValue("axis"), 0.0F);

    // Controls still held when input returns read as up until they are released, instead of pressing again.
    input.endFrame();
    actions.update(input, virtualInput, false);
    EXPECT_EQ(read(&ActionMap::isDown), "000000");
    EXPECT_EQ(actions.getVector("stick"), math::Vec2{});

    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyUp, Key::P), viewport);
    input.handleEvent(makeMouseEvent(platform::Event::Type::MouseUp, MouseButton::Right), viewport);
    gamepads[0] = GamepadState{.connected = true};
    input.updateGamepads(gamepads);
    virtualInput.clear();
    actions.update(input, virtualInput, false);
    EXPECT_EQ(read(&ActionMap::isDown), "000000");

    input.endFrame();
    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::P), viewport);
    input.handleEvent(makeMouseEvent(platform::Event::Type::MouseDown, MouseButton::Right), viewport);
    gamepads[0] = makeGamepad(GamepadButton::Start, 1.0F);
    input.updateGamepads(gamepads);
    virtualInput.setButton("attack", true);
    virtualInput.setStick("move", math::Vec2(1.0F, 0.0F));
    actions.update(input, virtualInput, false);
    EXPECT_EQ(read(&ActionMap::isPressed), "111111");
    EXPECT_EQ(actions.getVector("stick"), math::Vec2(1.0F, 0.0F));
}

TEST_F(ActionMapTest, RejectsInvalidDocuments) {
    ActionMap actions;
    EXPECT_THROW(actions.load(core::Json::parse(R"({"actions": [{"name": "a", "type": "trigger"}]})")), std::invalid_argument);
    EXPECT_THROW(actions.load(core::Json::parse(R"({"actions": [{"name": "a", "type": "button", "bindings": ["key:nope"]}]})")), std::invalid_argument);
    EXPECT_THROW(actions.load(core::Json::parse(R"({"other": []})")), std::invalid_argument);
    EXPECT_THROW(actions.load(core::Json::parse(R"({"actions": [{"name": "a", "type": "button", "binding": ["key:a"]}]})")), std::invalid_argument);
    EXPECT_THROW(actions.load(core::Json::parse(R"({"actions": []})").at("actions")), std::invalid_argument);
    EXPECT_THROW(actions.load(core::Json::parse(R"({"actions": {"jump": {}}})")), std::invalid_argument);
    EXPECT_THROW(actions.load(core::Json::parse(R"({"actions": [{"name": "a", "type": "button", "bindings": [7]}]})")), std::invalid_argument);
    EXPECT_NO_THROW(actions.load(core::Json::parse(R"({"actions": {}})")));

    // Every binding list must be one the action type reads.
    // clang-format off
    const auto message = [&actions](const char* document) {
        try {
            actions.load(core::Json::parse(document));
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }
        return std::string();
    };
    // clang-format on
    EXPECT_EQ(message(R"({"actions": [{"name": "jump", "type": "button", "bindings": ["key:space"], "positive": ["key:w"]}]})"), "The button action jump does not read positive.");
    EXPECT_EQ(message(R"({"actions": [{"name": "zoom", "type": "axis", "bindings": ["key:q"]}]})"), "The axis action zoom does not read bindings.");
    EXPECT_EQ(message(R"({"actions": [{"name": "move", "type": "vector", "negative": ["key:s"]}]})"), "The vector action move does not read negative.");
    EXPECT_EQ(message(R"({"actions": [{"name": "move", "type": "vector", "bindings": ["stick:left", "key:w"]}]})"), "The vector action move takes only sticks in bindings, not key:w.");
    EXPECT_THROW(actions.define({.name = "fire", .type = ActionMap::Action::Type::Button, .up = {*ActionMap::Binding::parse("key:w")}}), std::invalid_argument);
    EXPECT_EQ(actions.findAction("fire"), nullptr);
}

TEST(ViewportTest, FitLetterboxesAndConvertsCoordinates) {
    graphics::Viewport viewport;
    viewport.update(math::Vec2(1920.0F, 1200.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::Fit);
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{0.0F, 60.0F, 1920.0F, 1080.0F}));
    EXPECT_EQ(viewport.getVisibleRect(), (math::Rect{0.0F, 0.0F, 1920.0F, 1080.0F}));
    EXPECT_EQ(viewport.toDesign(math::Vec2(960.0F, 600.0F)), math::Vec2(960.0F, 540.0F));
    EXPECT_EQ(viewport.toFramebuffer(math::Vec2(0.0F, 0.0F)), math::Vec2(0.0F, 60.0F));
    EXPECT_EQ(viewport.getPolicy(), graphics::Viewport::ScalingPolicy::Fit);
    EXPECT_EQ(viewport.getDesignSize(), math::Vec2(1920.0F, 1080.0F));
    EXPECT_EQ(viewport.getFramebufferSize(), math::Vec2(1920.0F, 1200.0F));
}

TEST(ViewportTest, ExpandFillsTheScreenAroundTheDesignArea) {
    graphics::Viewport viewport;
    viewport.update(math::Vec2(2340.0F, 1080.0F), math::Vec2(1920.0F, 1080.0F), graphics::Viewport::ScalingPolicy::Expand, math::Insets{90.0F, 0.0F, 90.0F, 40.0F});
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{0.0F, 0.0F, 2340.0F, 1080.0F}));
    EXPECT_EQ(viewport.getVisibleRect(), (math::Rect{-210.0F, 0.0F, 2340.0F, 1080.0F}));
    EXPECT_EQ(viewport.getSafeRect(), (math::Rect{-120.0F, 0.0F, 2160.0F, 1040.0F}));
    EXPECT_EQ(viewport.getPixelsPerUnit(), math::Vec2(1.0F, 1.0F));
}

TEST(ViewportTest, FillStretchAndPixelPerfect) {
    graphics::Viewport viewport;
    viewport.update(math::Vec2(1000.0F, 1000.0F), math::Vec2(2000.0F, 1000.0F), graphics::Viewport::ScalingPolicy::Fill);
    EXPECT_EQ(viewport.getVisibleRect(), (math::Rect{500.0F, 0.0F, 1000.0F, 1000.0F}));

    viewport.update(math::Vec2(1000.0F, 1000.0F), math::Vec2(2000.0F, 1000.0F), graphics::Viewport::ScalingPolicy::Stretch);
    EXPECT_EQ(viewport.getPixelsPerUnit(), math::Vec2(0.5F, 1.0F));

    viewport.update(math::Vec2(1000.0F, 700.0F), math::Vec2(320.0F, 180.0F), graphics::Viewport::ScalingPolicy::PixelPerfect);
    EXPECT_EQ(viewport.getPixelRect(), (math::Rect{20.0F, 80.0F, 960.0F, 540.0F}));

    viewport.update(math::Vec2(0.0F, 0.0F), math::Vec2(0.0F, 0.0F), graphics::Viewport::ScalingPolicy::Fit);
    EXPECT_EQ(viewport.getFramebufferSize(), math::Vec2(1.0F, 1.0F));

    EXPECT_EQ(graphics::Viewport::scalingPolicyFromName("expand"), graphics::Viewport::ScalingPolicy::Expand);
    EXPECT_FALSE(graphics::Viewport::scalingPolicyFromName("zoom").has_value());
}

} // namespace haylen::input
