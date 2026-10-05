#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/GamepadState.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

// A test screen: a Back button, the play area and a panel with two controls, over an app that moves with the arrows, WASD, the d-pad and the left stick, jumps with Space and the south button, shoots with the left mouse button and pauses with Escape.
class PlayAreaTest : public ::testing::Test, public test::UiFixture {
  protected:
    PlayAreaTest() {
        // clang-format off
        getEngine().getActions().load(core::Json::parse(R"({"actions": [
            {"name": "move", "type": "vector", "left": ["key:left", "key:a", "button:dpadLeft"], "right": ["key:right", "key:d", "button:dpadRight"], "up": ["key:up", "key:w", "button:dpadUp"], "down": ["key:down", "key:s", "button:dpadDown"], "bindings": ["stick:left"]},
            {"name": "jump", "type": "button", "bindings": ["key:space", "button:south"]},
            {"name": "shoot", "type": "button", "bindings": ["mouse:left"]},
            {"name": "pause", "type": "button", "bindings": ["key:escape", "button:east"]}
        ]})"));
        gui = mount(R"({"kind": "column", "id": "screen", "padding": 20, "gap": 20, "children": [
            {"kind": "button", "id": "back", "text": "Back"},
            {"kind": "row", "grow": 1, "gap": 20, "children": [
                {"kind": "playArea", "id": "stage", "grow": 1, "align": "stretch"},
                {"kind": "column", "width": 400, "children": [{"kind": "button", "id": "first", "text": "First"}, {"kind": "button", "id": "second", "text": "Second"}]}
            ]}
        ]})");
        // clang-format on
    }

    void focus(const std::string& id) {
        gui->command(getUi().getContext(), id, "focus", core::Json::object());
        frames(2);
    }

    void hold(input::Key code, int count = 3) {
        press(code, true);
        frames(count);
    }
    void press(input::Key code, bool down) {
        platform::Event event;
        event.type = down ? platform::Event::Type::KeyDown : platform::Event::Type::KeyUp;
        event.key = code;
        getEngine().handleEvent(event);
    }
    void release(input::Key code) {
        press(code, false);
        frames();
    }

    [[nodiscard]] float getMove() {
        return getEngine().getActions().getVector("move").x;
    }
    [[nodiscard]] bool isDown(const char* action) {
        return getEngine().getActions().isDown(action);
    }
    [[nodiscard]] FocusNavigator::Owner getOwner() {
        return getUi().getFocus().getOwner();
    }

    std::shared_ptr<Gui> gui;
};

} // namespace

TEST_F(PlayAreaTest, GivesTheKeyboardToTheGameWhileItHasTheFocus) {
    focus("stage");
    ASSERT_EQ(getOwner(), FocusNavigator::Owner::PlayArea);

    hold(input::Key::Right);
    EXPECT_FLOAT_EQ(getMove(), 1.0F);
    EXPECT_TRUE(isFocused(*gui, "stage"));
    release(input::Key::Right);

    hold(input::Key::D);
    EXPECT_FLOAT_EQ(getMove(), 1.0F);
    release(input::Key::D);

    clearEvents();
    hold(input::Key::Space, 1);
    EXPECT_TRUE(isDown("jump"));
    release(input::Key::Space);
    EXPECT_TRUE(getEventNames().empty());

    // Escape stays the way back of the screen, which the game never hears while the play area has the focus.
    hold(input::Key::Escape, 2);
    EXPECT_FALSE(isDown("pause"));
    release(input::Key::Escape);
    EXPECT_EQ(findLastEvent("cancel").id, "screen");
}

TEST_F(PlayAreaTest, GivesTheKeyboardToTheControlThatHasTheFocus) {
    focus("first");
    ASSERT_EQ(getOwner(), FocusNavigator::Owner::Control);

    // The first direction only shows the ring, and the next ones move the focus while the game reads nothing.
    key(input::Key::Down);
    hold(input::Key::Down);
    EXPECT_FLOAT_EQ(getEngine().getActions().getVector("move").y, 0.0F);
    release(input::Key::Down);
    EXPECT_TRUE(isFocused(*gui, "second"));

    clearEvents();
    hold(input::Key::Space, 1);
    EXPECT_FALSE(isDown("jump"));
    release(input::Key::Space);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"second:click"}));
}

TEST_F(PlayAreaTest, MovesTheFocusWithTabAndTheFocusAction) {
    focus("stage");
    key(input::Key::Tab);
    EXPECT_TRUE(isFocused(*gui, "first"));
    key(input::Key::Tab);
    EXPECT_TRUE(isFocused(*gui, "second"));

    // The focus action goes to the play area and back to the control the player left.
    button(input::GamepadButton::Back);
    EXPECT_TRUE(isFocused(*gui, "stage"));
    button(input::GamepadButton::Back);
    EXPECT_TRUE(isFocused(*gui, "second"));

    // An app remaps it like any navigation action, such as to the pause key that the Play/Pause button of a TV remote sends.
    getEngine().getActions().define({.name = "uiFocus", .bindings = {*input::ActionMap::Binding::parse("key:pause")}});
    key(input::Key::Pause);
    EXPECT_TRUE(isFocused(*gui, "stage"));
    button(input::GamepadButton::Back);
    EXPECT_TRUE(isFocused(*gui, "stage"));
}

TEST_F(PlayAreaTest, GivesTheGamepadToTheGameOrToTheControls) {
    focus("stage");
    stick({1.0F, 0.0F});
    EXPECT_FLOAT_EQ(getMove(), 1.0F);
    stick({});
    button(input::GamepadButton::DpadLeft);
    EXPECT_TRUE(isFocused(*gui, "stage"));

    // A control with the focus takes the stick, the d-pad and the face buttons from the game.
    focus("first");
    stick({0.0F, 1.0F});
    EXPECT_FLOAT_EQ(getEngine().getActions().getVector("move").y, 0.0F);
    EXPECT_TRUE(getEngine().getActions().isGamepadAxisCaptured(0, input::GamepadAxis::LeftY));
    stick({});
    EXPECT_FALSE(getEngine().getActions().isGamepadAxisCaptured(0, input::GamepadAxis::LeftY));
    stick({0.0F, 1.0F});
    stick({});
    EXPECT_TRUE(isFocused(*gui, "second"));

    clearEvents();
    input::GamepadState south{.connected = true};
    south.buttons[static_cast<std::size_t>(input::GamepadButton::South)] = true;
    getFixture().host().setGamepad(0, south);
    frames(2);
    EXPECT_FALSE(isDown("jump"));
    getFixture().host().setGamepad(0, input::GamepadState{.connected = true});
    frames(2);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"second:click"}));
}

TEST_F(PlayAreaTest, TakesTheFocusFromAClickOrATouchThatTheGameAlsoReads) {
    focus("first");
    const math::Vec2 center = getBounds(*gui, "stage").getCenter();
    pointer(platform::Event::Type::MouseMove, center);
    frames();
    pointer(platform::Event::Type::MouseDown, center);
    frames();
    EXPECT_TRUE(isFocused(*gui, "stage"));
    EXPECT_TRUE(isDown("shoot"));
    pointer(platform::Event::Type::MouseUp, center);
    frames();

    focus("first");
    touch(platform::Event::Type::TouchBegan, 7, center);
    frames(2);
    touch(platform::Event::Type::TouchEnded, 7, center);
    frames(2);
    EXPECT_TRUE(isFocused(*gui, "stage"));

    // A click on a control takes the focus to the control.
    click(*gui, "back");
    EXPECT_TRUE(isFocused(*gui, "back"));
}

TEST_F(PlayAreaTest, KeepsAPressWithTheSideThatTookItUntilItIsReleased) {
    focus("stage");
    hold(input::Key::Right);
    ASSERT_FLOAT_EQ(getMove(), 1.0F);

    // The press that started in the game keeps moving it, and the controls never see it, however long it repeats.
    key(input::Key::Tab);
    frames(60);
    EXPECT_TRUE(isFocused(*gui, "first"));
    EXPECT_FLOAT_EQ(getMove(), 1.0F);
    release(input::Key::Right);
    EXPECT_FLOAT_EQ(getMove(), 0.0F);

    // A press that started on a control stays with the controls once the focus moved to the game.
    hold(input::Key::Down);
    button(input::GamepadButton::Back);
    ASSERT_TRUE(isFocused(*gui, "stage"));
    EXPECT_FLOAT_EQ(getEngine().getActions().getVector("move").y, 0.0F);
    release(input::Key::Down);
}

TEST_F(PlayAreaTest, NavigatesWithATelevisionRemote) {
    getFixture().host().setPointerDevice(false);
    focus("stage");
    EXPECT_TRUE(getUi().getFocus().isRingVisible());

    // The remote moves the game while the play area has the focus, and the focus action takes it to the first control of the screen, which select presses.
    button(input::GamepadButton::DpadRight);
    EXPECT_TRUE(isFocused(*gui, "stage"));
    button(input::GamepadButton::Back);
    EXPECT_TRUE(isFocused(*gui, "back"));
    button(input::GamepadButton::South);
    EXPECT_EQ(findLastEvent("click").id, "back");
}

TEST_F(PlayAreaTest, ReportsTheOwnerOfTheFocusToLua) {
    EXPECT_EQ(getFixture().lua("return require('haylen.ui').focusOwner()"), "none");
    focus("stage");
    EXPECT_EQ(getFixture().lua("return require('haylen.ui').focusOwner()"), "playArea");
    focus("back");
    EXPECT_EQ(getFixture().lua("return require('haylen.ui').focusOwner()"), "control");

    stick({1.0F, 0.0F});
    EXPECT_EQ(getFixture().lua("return tostring(require('haylen.input').gamepadAxisCaptured('leftX'))"), "true");
    stick({});
}

} // namespace haylen::ui
