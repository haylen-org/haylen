#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <string>

#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/ActionMaps.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/platform/Event.hpp"
#include "input/InputFixture.hpp"
#include "support/ActionBindings.hpp"

namespace haylen::input {

class ActionMapTest : public InputFixture {};

TEST_F(ActionMapTest, LoadsSavesAndReportsActions) {
    // clang-format off
    const core::Json document = core::Json::parse(R"({
        "actions": [
            {"name": "attack", "type": "button", "bindings": ["key:space", "mouse:left", "button:south", "virtual:attack"]},
            {"name": "zoom", "type": "axis", "positive": ["key:e", "axis:rightTrigger+"], "negative": ["key:q"]},
            {"name": "move", "type": "vector", "up": ["key:w"], "down": ["key:s"], "left": ["key:a"], "right": ["key:d"], "bindings": ["stick:left", "virtualStick:move"]}
        ]
    })");
    // clang-format on

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
    actions.define({.name = "attack", .type = ActionMap::Action::Type::Button, .bindings = {test::ActionBindings::parse("virtual:attack"), test::ActionBindings::parse("button:east")}});
    actions.define({.name = "move", .type = ActionMap::Action::Type::Vector, .bindings = {test::ActionBindings::parse("virtualStick:move"), test::ActionBindings::parse("stick:left")}});
    actions.define({.name = "aim", .type = ActionMap::Action::Type::Axis, .positive = {test::ActionBindings::parse("axis:leftX+")}});

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

    actions.setDevices({.gamepads = ActionMap::Devices::Gamepads::One, .gamepad = 0});
    actions.update(input, virtualInput, false);
    EXPECT_FALSE(actions.isDown("attack"));
    EXPECT_EQ(actions.getVector("move"), math::Vec2{});
    EXPECT_EQ(actions.getValue("aim"), 0.0F);

    actions.setDevices({});
    actions.update(input, virtualInput, false);
    EXPECT_TRUE(actions.isDown("attack"));

    actions.define({.name = "attack", .type = ActionMap::Action::Type::Button});
    const ActionMap::Action* attack = actions.findAction("attack");
    ASSERT_NE(attack, nullptr);
    EXPECT_TRUE(attack->bindings.empty());
    EXPECT_EQ(actions.getNames().front(), "attack");
    actions.remove("attack");
    EXPECT_EQ(actions.findAction("attack"), nullptr);
    actions.clear();
    EXPECT_TRUE(actions.getNames().empty());
}

TEST_F(ActionMapTest, PressThresholdDecidesWhenAnActionIsDown) {
    ActionMap actions;
    actions.define({.name = "aim", .type = ActionMap::Action::Type::Axis, .positive = {test::ActionBindings::parse("axis:leftX+")}});
    Input input;
    VirtualInput virtualInput;
    std::array<GamepadState, 1> states{makeGamepad(GamepadButton::South, 0.7F)};
    input.updateGamepads(states);
    actions.update(input, virtualInput, false);
    EXPECT_TRUE(actions.isDown("aim"));

    actions.setPressThreshold(0.75F);
    actions.update(input, virtualInput, false);
    EXPECT_FALSE(actions.isDown("aim"));

    // A threshold of 0 or less would hold every action down forever, and one above 1 would never press any.
    EXPECT_THROW(actions.setPressThreshold(0.0F), std::invalid_argument);
    EXPECT_THROW(actions.setPressThreshold(1.5F), std::invalid_argument);
    actions.setPressThreshold(1.0F);
    states[0] = makeGamepad(GamepadButton::South, 1.0F);
    input.updateGamepads(states);
    actions.update(input, virtualInput, false);
    EXPECT_TRUE(actions.isPressed("aim"));
}

TEST_F(ActionMapTest, BlockedInputHoldsEveryControlUntilItIsReleased) {
    ActionMap actions;
    actions.define({.name = "key", .type = ActionMap::Action::Type::Button, .bindings = {test::ActionBindings::parse("key:p")}});
    actions.define({.name = "mouse", .type = ActionMap::Action::Type::Button, .bindings = {test::ActionBindings::parse("mouse:right")}});
    actions.define({.name = "button", .type = ActionMap::Action::Type::Button, .bindings = {test::ActionBindings::parse("button:start")}});
    actions.define({.name = "axis", .type = ActionMap::Action::Type::Axis, .positive = {test::ActionBindings::parse("axis:leftX+")}});
    actions.define({.name = "virtual", .type = ActionMap::Action::Type::Button, .bindings = {test::ActionBindings::parse("virtual:attack")}});
    actions.define({.name = "stick", .type = ActionMap::Action::Type::Vector, .bindings = {test::ActionBindings::parse("virtualStick:move")}});
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

TEST_F(ActionMapTest, GivesAKeyPressedAgainWithinAFrameToTheSideThatHoldsItNow) {
    ActionMap actions;
    actions.define({.name = "back", .type = ActionMap::Action::Type::Button, .bindings = {test::ActionBindings::parse("key:escape")}});
    Input input;
    VirtualInput virtualInput;
    const graphics::Viewport viewport = makeIdentityViewport();
    ActionMap::Capture capture;
    capture.keys.set(static_cast<std::size_t>(Key::Escape));
    actions.setCapture(capture);
    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::Escape), viewport);
    actions.update(input, virtualInput, false);
    EXPECT_FALSE(actions.isDown("back"));

    // The UI lets go, and the key goes up and down again within one frame.
    actions.setCapture({});
    input.endFrame();
    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyUp, Key::Escape), viewport);
    input.handleEvent(makeKeyEvent(platform::Event::Type::KeyDown, Key::Escape), viewport);
    actions.update(input, virtualInput, false);
    EXPECT_TRUE(actions.isPressed("back"));
}

TEST_F(ActionMapTest, DeliversEachPressToExactlyOneFixedStep) {
    ActionMap actions;
    actions.define({.name = "jump", .type = ActionMap::Action::Type::Button, .bindings = {test::ActionBindings::parse("virtual:jump")}});
    Input input;
    VirtualInput virtualInput;
    // clang-format off
    const auto frame = [&](bool down) {
        virtualInput.setButton("jump", down);
        actions.update(input, virtualInput, false);
    };
    // clang-format on

    // A press in a frame that runs no step reaches the first step of the next frame, and only that one.
    frame(true);
    EXPECT_FALSE(actions.isStepPressed("jump"));
    frame(true);
    actions.beginStep(1.0F / 30.0F);
    EXPECT_TRUE(actions.isStepPressed("jump"));
    EXPECT_TRUE(actions.isStepDown("jump"));
    actions.beginStep(1.0F / 30.0F);
    EXPECT_FALSE(actions.isStepPressed("jump"));
    EXPECT_TRUE(actions.isStepDown("jump"));

    // A tap within one frame holds the action for one step and releases it in the next one.
    frame(false);
    actions.beginStep(1.0F / 30.0F);
    EXPECT_TRUE(actions.isStepReleased("jump"));
    frame(true);
    frame(false);
    actions.beginStep(1.0F / 30.0F);
    EXPECT_TRUE(actions.isStepPressed("jump"));
    EXPECT_TRUE(actions.isStepDown("jump"));
    EXPECT_FALSE(actions.isStepReleased("jump"));
    actions.beginStep(1.0F / 30.0F);
    EXPECT_FALSE(actions.isStepDown("jump"));
    EXPECT_TRUE(actions.isStepReleased("jump"));
    actions.beginStep(1.0F / 30.0F);
    EXPECT_FALSE(actions.isStepReleased("jump"));

    // Blocked input drops the edges that no step saw yet.
    frame(true);
    actions.update(input, virtualInput, true);
    actions.beginStep(1.0F / 30.0F);
    EXPECT_FALSE(actions.isStepPressed("jump"));
    EXPECT_FALSE(actions.isStepDown("missing"));
}

TEST_F(ActionMapTest, ConsumesEachPressOnceWithinItsBuffer) {
    ActionMap actions;
    actions.load(core::Json::parse(R"({"actions": [{"name": "jump", "type": "button", "bindings": ["virtual:jump"], "buffer": 0.1}, {"name": "fire", "type": "button", "bindings": ["virtual:fire"]}]})"));
    EXPECT_EQ(actions.save().at("actions").at(0).at("buffer"), 0.1);
    EXPECT_FALSE(actions.save().at("actions").at(1).contains("buffer"));
    Input input;
    VirtualInput virtualInput;
    virtualInput.setButton("jump", true);
    virtualInput.setButton("fire", true);
    actions.update(input, virtualInput, false);

    // A press waits for the first step after it, and the buffer keeps it for its seconds of steps beyond that one.
    constexpr float kStep = 1.0F / 60.0F;
    actions.beginStep(kStep);
    actions.beginStep(kStep);
    EXPECT_FALSE(actions.consumePress("fire"));

    for (int step = 0; step < 5; ++step) {
        actions.beginStep(kStep);
    }

    EXPECT_TRUE(actions.consumePress("jump"));
    EXPECT_FALSE(actions.consumePress("jump"));

    virtualInput.clear();
    actions.update(input, virtualInput, false);
    virtualInput.setButton("jump", true);
    actions.update(input, virtualInput, false);
    for (int step = 0; step < 8; ++step) {
        actions.beginStep(kStep);
    }

    EXPECT_FALSE(actions.consumePress("jump"));

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
    EXPECT_EQ(message(R"({"actions": [{"name": "jump", "type": "button", "buffer": -1}]})"), "The buffer of an action must be a number of seconds from 0.");
    EXPECT_EQ(message(R"({"actions": [{"name": "jump", "type": "button", "buffer": "long"}]})"), "The buffer of an action must be a number of seconds from 0.");
}

TEST_F(ActionMapTest, EachMapReadsOnlyItsDevices) {
    const core::Json document = core::Json::parse(R"({"actions": [{"name": "jump", "type": "button", "bindings": ["key:space", "button:south", "virtual:jump"]}, {"name": "move", "type": "vector", "bindings": ["stick:left"]}]})");
    ActionMaps maps;
    const std::shared_ptr<ActionMap> keyboard = maps.create({.mouse = false, .gamepads = ActionMap::Devices::Gamepads::None, .virtualPrefix = "touch."});
    std::shared_ptr<ActionMap> second = maps.create({.keyboard = false, .mouse = false, .gamepads = ActionMap::Devices::Gamepads::One, .gamepad = 1, .virtualControls = false});
    const std::shared_ptr<ActionMap> eighth = maps.create({.keyboard = false, .gamepads = ActionMap::Devices::Gamepads::One, .gamepad = 7, .virtualControls = false});

    for (const auto& map : {maps.getDefault(), keyboard, second, eighth}) {
        map->load(document);
    }

    EXPECT_EQ(maps.size(), 4U);

    Input input;
    VirtualInput virtualInput;
    std::array<GamepadState, Input::kMaxGamepads> pads{};
    pads[1] = makeGamepad(GamepadButton::South, 1.0F);
    input.updateGamepads(pads);
    maps.update(input, virtualInput, false);
    EXPECT_TRUE(maps.getDefault()->isDown("jump"));
    EXPECT_FALSE(keyboard->isDown("jump"));
    EXPECT_TRUE(second->isDown("jump"));
    EXPECT_NEAR(second->getVector("move").x, 1.0F, 1e-5F);
    EXPECT_FALSE(eighth->isDown("jump"));

    // The last of the eight gamepads drives only its own map, and the virtual controls reach the map whose prefix they carry.
    pads[1] = {};
    pads[7] = makeGamepad(GamepadButton::South);
    input.updateGamepads(pads);
    virtualInput.setButton("touch.jump", true);
    maps.update(input, virtualInput, false);
    EXPECT_TRUE(eighth->isPressed("jump"));
    EXPECT_FALSE(second->isDown("jump"));
    EXPECT_TRUE(keyboard->isPressed("jump"));
    virtualInput.setButton("jump", true);
    maps.update(input, virtualInput, false);
    EXPECT_FALSE(second->isDown("jump"));

    // The presses that the UI captures stay away from every map, and a map nobody holds leaves at the next update.
    ActionMap::Capture capture;
    capture.buttons.set(static_cast<std::size_t>(GamepadButton::South));
    capture.gamepads.set();
    maps.setCapture(capture);
    pads[7] = {};
    input.updateGamepads(pads);
    maps.update(input, virtualInput, false);
    pads[7] = makeGamepad(GamepadButton::South);
    input.updateGamepads(pads);
    maps.update(input, virtualInput, false);
    EXPECT_FALSE(eighth->isDown("jump"));
    const std::weak_ptr<ActionMap> released = second;
    second.reset();
    maps.update(input, virtualInput, false);
    EXPECT_TRUE(released.expired());
    EXPECT_EQ(maps.size(), 3U);
    EXPECT_THROW(keyboard->setDevices({.gamepads = ActionMap::Devices::Gamepads::One, .gamepad = Input::kMaxGamepads}), std::invalid_argument);
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
    EXPECT_EQ(message(R"({"actions": [{"name": "jump", "type": "button", "bindings": ["key:space"], "positive": ["key:w"]}]})"), "The button action \"jump\" does not read \"positive\".");
    EXPECT_EQ(message(R"({"actions": [{"name": "zoom", "type": "axis", "bindings": ["key:q"]}]})"), "The axis action \"zoom\" does not read \"bindings\".");
    EXPECT_EQ(message(R"({"actions": [{"name": "move", "type": "vector", "negative": ["key:s"]}]})"), "The vector action \"move\" does not read \"negative\".");
    EXPECT_EQ(message(R"({"actions": [{"name": "move", "type": "vector", "bindings": ["stick:left", "key:w"]}]})"), "The vector action \"move\" takes only sticks in \"bindings\", not \"key:w\".");
    EXPECT_EQ(message(R"({"actions": [{"name": "jump", "type": "button"}, {"name": "jump", "type": "axis"}]})"), "The action name \"jump\" is used by more than one action.");
    EXPECT_THROW(actions.define({.name = "fire", .type = ActionMap::Action::Type::Button, .up = {test::ActionBindings::parse("key:w")}}), std::invalid_argument);
    EXPECT_EQ(actions.findAction("fire"), nullptr);
}

} // namespace haylen::input
