#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class FocusTest : public ::testing::Test, public test::UiFixture {
  protected:
    [[nodiscard]] bool isRingVisible() {
        return getUi().getFocus().isRingVisible();
    }
};

} // namespace

TEST_F(FocusTest, MovesBetweenNeighborsWithArrowsTheDirectionalPadAndTheStick) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "row", "children": [{"kind": "button", "id": "a", "text": "Alpha", "autofocus": true}, {"kind": "button", "id": "b", "text": "Beta"}]},
        {"kind": "row", "children": [{"kind": "button", "id": "c", "text": "Gamma"}, {"kind": "button", "id": "d", "text": "Delta"}]}
    ]})");
    // clang-format on
    ASSERT_TRUE(isFocused(*document, "a"));
    EXPECT_FALSE(isRingVisible());

    // The first press of a player who could not see the ring only shows it.
    key(input::Key::Right);
    EXPECT_TRUE(isRingVisible());
    EXPECT_TRUE(isFocused(*document, "a"));
    key(input::Key::Right);
    EXPECT_TRUE(isFocused(*document, "b"));
    key(input::Key::Down);
    EXPECT_TRUE(isFocused(*document, "d"));
    key(input::Key::Left);
    EXPECT_TRUE(isFocused(*document, "c"));

    button(input::GamepadButton::DpadUp);
    EXPECT_TRUE(isFocused(*document, "a"));
    stick({1.0F, 0.0F});
    stick({});
    EXPECT_TRUE(isFocused(*document, "b"));

    // Nothing lies further right, so the focus stays.
    key(input::Key::Right);
    EXPECT_TRUE(isFocused(*document, "b"));
}

TEST_F(FocusTest, FollowsExplicitNeighborsAndWrapsAround) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "a", "text": "A", "autofocus": true, "focusDown": "c"},
        {"kind": "button", "id": "b", "text": "B"},
        {"kind": "button", "id": "c", "text": "C", "focusDown": "c"},
        {"kind": "row", "focusWrap": "horizontal", "children": [{"kind": "button", "id": "x", "text": "X"}, {"kind": "button", "id": "y", "text": "Y"}, {"kind": "button", "id": "z", "text": "Z"}]}
    ]})");
    // clang-format on
    key(input::Key::Down);
    key(input::Key::Down);
    EXPECT_TRUE(isFocused(*document, "c"));

    // A node that names itself keeps the focus in that direction.
    key(input::Key::Down);
    EXPECT_TRUE(isFocused(*document, "c"));

    document->command(getUi().getContext(), "z", "focus", core::Json::object());
    frames(2);
    key(input::Key::Right);
    EXPECT_TRUE(isFocused(*document, "x"));
    key(input::Key::Left);
    EXPECT_TRUE(isFocused(*document, "z"));
    key(input::Key::Up);
    EXPECT_TRUE(isFocused(*document, "c"));
}

TEST_F(FocusTest, KeepsTheFocusInsideScopesAndSendsThemCancel) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "id": "screen", "padding": 20, "children": [
        {"kind": "column", "id": "menu", "focusScope": true, "children": [{"kind": "button", "id": "first", "text": "First"}, {"kind": "button", "id": "second", "text": "Second"}]},
        {"kind": "button", "id": "outside", "text": "Outside"}
    ]})");
    // clang-format on
    document->command(getUi().getContext(), "first", "focus", core::Json::object());
    frames(2);
    key(input::Key::Down);
    key(input::Key::Down);
    EXPECT_TRUE(isFocused(*document, "second"));
    key(input::Key::Down);
    EXPECT_TRUE(isFocused(*document, "second"));

    key(input::Key::Escape);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"first:focus", "first:blur", "second:focus", "menu:cancel"}));

    // Navigation never enters a scope from outside, and cancel outside every scope goes to the root.
    document->command(getUi().getContext(), "outside", "focus", core::Json::object());
    frames(2);
    key(input::Key::Up);
    EXPECT_TRUE(isFocused(*document, "outside"));
    frames(2);
    clearEvents();
    button(input::GamepadButton::East);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"screen:cancel"}));
}

TEST_F(FocusTest, SendsCancelToTheTopmostDocumentWithoutFocus) {
    mount(R"({"kind": "column", "id": "hud", "children": [{"kind": "label", "text": "HUD"}]})");
    mount(R"({"kind": "column", "id": "pause", "children": [{"kind": "label", "text": "Paused"}]})", Placement::Screen, 1);
    key(input::Key::Escape);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"pause:cancel"}));
}

TEST_F(FocusTest, RestoresTheFocusWhenADialogCloses) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "quit", "text": "Quit", "autofocus": true},
        {"kind": "button", "id": "stay", "text": "Stay"},
        {"kind": "dialog", "id": "confirm", "title": "Quit?", "buttons": [{"id": "no", "text": "No"}, {"id": "yes", "text": "Yes"}]}
    ]})");
    // clang-format on
    key(input::Key::Down);
    ASSERT_TRUE(isFocused(*document, "quit"));
    ASSERT_TRUE(isRingVisible());

    document->set("confirm", {{"open", true}});
    frames(2);
    EXPECT_TRUE(getEngine().isBackCaptured());
    key(input::Key::Left);
    key(input::Key::Enter);
    frames(2);
    EXPECT_EQ(findLastEvent("answer").value, (core::Json{{"button", "no"}}));
    EXPECT_TRUE(isFocused(*document, "quit"));
    EXPECT_FALSE(getEngine().isBackCaptured());
}

TEST_F(FocusTest, OpensPopoversWithTheFocusInsideAndCancelClosesThem) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "id": "screen", "padding": 20, "children": [
        {"kind": "popover", "id": "help", "text": "Help", "autofocus": true, "children": [{"kind": "column", "children": [{"kind": "button", "id": "inner", "text": "Inner"}, {"kind": "button", "id": "more", "text": "More"}]}]}
    ]})");
    // clang-format on
    key(input::Key::Enter);
    frames(2);
    EXPECT_TRUE(isFocused(*document, "inner"));
    key(input::Key::Down);
    EXPECT_TRUE(isFocused(*document, "more"));

    frames(2);
    clearEvents();
    key(input::Key::Escape);
    frames();
    EXPECT_TRUE(isFocused(*document, "help"));
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"help:focus"}));
}

TEST_F(FocusTest, ScrollsTheFocusedControlIntoView) {
    std::string buttons;
    for (int index = 0; index < 12; ++index) {
        buttons += (index > 0 ? ", " : "") + std::string(R"({"kind": "button", "id": "b)") + std::to_string(index) + R"(", "text": "Item"})";
    }
    auto document = mount(R"({"kind": "scroll", "id": "list", "height": 300, "width": 400, "align": "start", "children": [{"kind": "column", "children": [)" + buttons + R"(]}]})");
    document->command(getUi().getContext(), "b0", "focus", core::Json::object());
    frames(2);
    key(input::Key::Down);
    for (int index = 0; index < 9; ++index) {
        key(input::Key::Down);
    }
    frames(2);
    ASSERT_TRUE(isFocused(*document, "b9"));
    const math::Rect list = getBounds(*document, "list");
    const math::Rect focused = getBounds(*document, "b9");
    EXPECT_GE(focused.y, list.y);
    EXPECT_LE(focused.getBottom(), list.getBottom() + 0.5F);
}

TEST_F(FocusTest, MovesSlidersWithLeftAndRight) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "slider", "id": "volume", "value": 0.5, "step": 0.1, "autofocus": true},
        {"kind": "button", "id": "done", "text": "Done"}
    ]})");
    // clang-format on
    key(input::Key::Right);
    key(input::Key::Right);
    EXPECT_NEAR(findLastEvent("change").value.at("value").get<double>(), 0.6, 1e-9);
    key(input::Key::Left);
    key(input::Key::Left);
    EXPECT_NEAR(findLastEvent("change").value.at("value").get<double>(), 0.4, 1e-9);
    key(input::Key::Down);
    EXPECT_TRUE(isFocused(*document, "done"));
}

TEST_F(FocusTest, TabsFromFieldToFieldWhileTyping) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "textField", "id": "name"},
        {"kind": "textField", "id": "city"}
    ]})");
    // clang-format on
    document->command(getUi().getContext(), "name", "focus", core::Json::object());
    frames(2);
    type(U"ab");
    key(input::Key::Tab);
    frames();
    type(U"cd");
    frames();
    EXPECT_TRUE(isFocused(*document, "city"));
    EXPECT_EQ(findLastEvent("change").id, "city");
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "cd"}}));
}

TEST_F(FocusTest, TakesAcceptFromTheActionMapWhenTheAppRemapsIt) {
    getEngine().getActions().load(core::Json::parse(R"({"actions": [{"name": "uiAccept", "type": "button", "bindings": ["key:x"]}]})"));
    auto document = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "play", "text": "Play", "autofocus": true}]})");
    key(input::Key::Enter);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"play:focus"}));
    key(input::Key::X);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"play:focus", "play:click"}));
    EXPECT_TRUE(isFocused(*document, "play"));
}

TEST_F(FocusTest, ShowsTheRingFromTheStartWithoutAPointerDevice) {
    getFixture().host().setPointerDevice(false);
    auto document = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "play", "text": "Play", "autofocus": true}]})");
    frames();
    EXPECT_TRUE(isFocused(*document, "play"));
    EXPECT_TRUE(isRingVisible());
}

TEST_F(FocusTest, DrivesCollectionsAndChoicesWithoutAPointer) {
    getFixture().host().setPointerDevice(false);
    // clang-format off
    auto document = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "tabs", "id": "tabs", "items": [{"id": "one", "text": "One"}, {"id": "two", "text": "Two"}], "children": [{"kind": "label", "text": "1"}, {"kind": "label", "text": "2"}]},
        {"kind": "list", "id": "list", "items": [{"id": "a", "text": "Alpha"}, {"id": "b", "text": "Beta"}]},
        {"kind": "tree", "id": "tree", "items": [{"id": "root", "text": "Root", "children": [{"id": "leaf", "text": "Leaf"}]}]},
        {"kind": "checkbox", "id": "music", "text": "Music"},
        {"kind": "splitter", "id": "split", "height": 60, "children": [{"kind": "spacer"}, {"kind": "spacer"}]}
    ]})");
    // clang-format on
    document->command(getUi().getContext(), "list", "focus", core::Json::object());
    frames(2);
    key(input::Key::Down);
    button(input::GamepadButton::South);
    EXPECT_EQ(findLastEvent("select").value, (core::Json{{"item", "b"}}));

    // Right opens a tree row and left closes it, while up and down walk the rows.
    key(input::Key::Down);
    key(input::Key::Right);
    EXPECT_EQ(findLastEvent("toggle").value, (core::Json{{"item", "root"}, {"expanded", true}}));
    key(input::Key::Down);
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("select").value, (core::Json{{"item", "leaf"}}));
    key(input::Key::Up);
    key(input::Key::Left);
    EXPECT_EQ(findLastEvent("toggle").value, (core::Json{{"item", "root"}, {"expanded", false}}));

    key(input::Key::Down);
    button(input::GamepadButton::South);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"checked", true}}));
    key(input::Key::Down);
    key(input::Key::Right);
    EXPECT_NEAR(findLastEvent("resize").value.at("ratio").get<double>(), 0.55, 1e-6);

    // The tabs above the list take the focus and switch with accept.
    document->command(getUi().getContext(), "list", "focus", core::Json::object());
    frames(2);
    key(input::Key::Up);
    key(input::Key::Up);
    key(input::Key::Left);
    key(input::Key::Right);
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("select").value, (core::Json{{"item", "two"}}));
    EXPECT_EQ(findLastEvent("select").id, "tabs");
}

TEST_F(FocusTest, SkipsControlsThatCannotTakeTheFocus) {
    // clang-format off
    auto document = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "a", "text": "A", "autofocus": true},
        {"kind": "row", "focusable": false, "children": [{"kind": "button", "id": "pause", "text": "Pause"}]},
        {"kind": "button", "id": "c", "text": "C"}
    ]})");
    // clang-format on
    key(input::Key::Down);
    key(input::Key::Down);
    EXPECT_TRUE(isFocused(*document, "c"));

    // A click presses the control without taking the focus, so a key that also plays the game never presses it again.
    click(*document, "pause");
    EXPECT_EQ(getLastEvent().name, "click");
    EXPECT_TRUE(isFocused(*document, "c"));
}

TEST_F(FocusTest, LetsTheAppKeepTheBackButton) {
    EXPECT_FALSE(getEngine().isBackCaptured());
    getEngine().setBackLeavesApp(false);
    EXPECT_FALSE(getEngine().canBackLeaveApp());
    EXPECT_TRUE(getEngine().isBackCaptured());
    getEngine().setBackLeavesApp(true);

    auto document = mount(R"({"kind": "popover", "id": "help", "text": "Help", "children": [{"kind": "label", "text": "Inside"}]})");
    click(*document, "help");
    EXPECT_TRUE(getEngine().isBackCaptured());
}

TEST_F(FocusTest, ClearsTheFocus) {
    auto document = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "play", "text": "Play", "autofocus": true}]})");
    ASSERT_TRUE(isFocused(*document, "play"));
    getUi().getBackend().makeCurrent();
    getUi().getFocus().clear();
    frames();
    EXPECT_EQ(getUi().getFocus().getFocusedDocument(), nullptr);
    key(input::Key::Enter);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"play:focus", "play:blur"}));
}

} // namespace haylen::ui
