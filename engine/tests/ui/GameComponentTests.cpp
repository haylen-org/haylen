#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/ui/FocusNavigator.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/UiFixture.hpp"
#include "ui/components/inputs/KeyCapture.hpp"

namespace haylen::ui {

namespace {

class GameComponentTest : public ::testing::Test, public test::UiFixture {
  protected:
    void focus(Gui& gui, const std::string& id) {
        gui.command(getUi().getContext(), id, "focus", core::Json::object());
        frames(2);
    }

    [[nodiscard]] std::size_t count(const std::string& name) const {
        const std::vector<std::string> names = getEventNames();
        return static_cast<std::size_t>(std::ranges::count_if(names, [&](const std::string& entry) { return entry.ends_with(":" + name); }));
    }

    [[nodiscard]] float getMetric(Theme::Metric role) {
        return getUi().getTheme().getMetric(role);
    }
};

} // namespace

TEST_F(GameComponentTest, DrawsRingsAndCooldowns) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "children": [
        {"kind": "circularProgress", "id": "ring", "value": 0.25, "text": "25%", "tone": "success"},
        {"kind": "circularProgress", "id": "cooldown", "value": 0.6, "variant": "cooldown", "size": 96, "text": "3"}
    ]})");
    // clang-format on
    frames(2);
    EXPECT_EQ(getEngine().getError(), nullptr);
    EXPECT_EQ(getBounds(*gui, "ring").getSize(), math::Vec2(72.0F, 72.0F));
    EXPECT_EQ(getBounds(*gui, "cooldown").getSize(), math::Vec2(96.0F, 96.0F));
    EXPECT_THROW(gui->set("ring", {{"value", 2}}), std::invalid_argument);
    EXPECT_THROW(gui->set("ring", {{"variant", "bar"}}), std::invalid_argument);
}

TEST_F(GameComponentTest, StepsNumbersAndOptions) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "stepper", "id": "players", "value": 2, "min": 1, "max": 4},
        {"kind": "stepper", "id": "difficulty", "items": [{"id": "easy", "text": "Easy"}, {"id": "normal", "text": "Normal"}, {"id": "hard", "text": "Hard", "enabled": false}], "selected": "normal"}
    ]})");
    // clang-format on
    const math::Rect players = getBounds(*gui, "players");
    click({players.getRight() - players.height * 0.5F, players.getCenter().y});
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", 3.0}}));
    for (int press = 0; press < 3; ++press) {
        click({players.x + players.height * 0.5F, players.getCenter().y});
    }
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", 1.0}}));
    EXPECT_EQ(count("change"), 3U);

    focus(*gui, "difficulty");
    key(input::Key::Left);
    key(input::Key::Left);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "easy"}}));
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "normal"}}));

    // A disabled option is skipped, and the end stops the stepper unless it wraps.
    key(input::Key::Right);
    EXPECT_EQ(count("change"), 5U);
    const math::Rect difficulty = getBounds(*gui, "difficulty");
    click(difficulty.getCenter());
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "easy"}}));
    EXPECT_THROW(gui->set("players", {{"step", 0}}), std::invalid_argument);
}

TEST_F(GameComponentTest, PicksSegments) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "segmentedControl", "id": "view", "width": 600, "items": [
        {"id": "list", "text": "List"}, {"id": "grid", "text": "Grid"}, {"id": "map", "text": "Map", "enabled": false}, {"id": "stats", "text": "Stats"}
    ], "selected": "list"}]})");
    // clang-format on
    const math::Rect view = getBounds(*gui, "view");
    click({view.x + view.width * 0.375F, view.getCenter().y});
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "grid"}}));
    click({view.x + view.width * 0.625F, view.getCenter().y});
    EXPECT_EQ(count("change"), 1U);

    focus(*gui, "view");
    key(input::Key::Right);
    key(input::Key::Right);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "stats"}}));
    key(input::Key::Right);
    EXPECT_EQ(count("change"), 2U);
    button(input::GamepadButton::South);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "list"}}));

    // Without a selection, accept reaches the last segment too.
    gui->set("view", {{"items", core::Json::parse(R"([{"id": "list", "text": "List", "enabled": false}, {"id": "grid", "text": "Grid"}])")}, {"selected", ""}});
    frames();
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "grid"}}));
}

TEST_F(GameComponentTest, DragsAndStepsBothEndsOfARange) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "rangeSlider", "id": "price", "width": 420, "min": 0, "max": 100, "low": 20, "high": 80, "step": 10}]})");
    const math::Rect price = getBounds(*gui, "price");
    const float knob = getMetric(Theme::Metric::SliderKnobSize);
    const auto at = [&](float amount) { return math::Vec2{price.x + knob * 0.5F + (price.width - knob) * amount, price.getCenter().y}; };
    drag(at(0.8F), at(0.5F));
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"low", 20.0}, {"high", 50.0}}));

    // The knob the pointer moved stays active for the keyboard, and accept switches to the other one.
    focus(*gui, "price");
    key(input::Key::Right);
    key(input::Key::Right);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"low", 20.0}, {"high", 60.0}}));
    key(input::Key::Enter);
    key(input::Key::Left);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"low", 10.0}, {"high", 60.0}}));
    EXPECT_THROW(gui->set("price", {{"low", 90}}), std::invalid_argument);
    EXPECT_THROW(gui->set("price", {{"max", 1e300}}), std::invalid_argument);

    // An end set past the other end where the player left it takes that end along.
    gui->set("price", {{"low", 70}});
    key(input::Key::Enter);
    key(input::Key::Right);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"low", 70.0}, {"high", 80.0}}));
}

TEST_F(GameComponentTest, MovesClosesAndCancelsWindows) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "label", "id": "title", "text": "Game"},
        {"kind": "button", "id": "menu", "text": "Menu"},
        {"kind": "window", "id": "bag", "title": "Bag", "x": 200, "y": 100, "width": 500, "closable": true, "children": [{"kind": "button", "id": "use", "text": "Use"}]}
    ]})");
    // clang-format on
    frames(2);
    EXPECT_EQ(getBounds(*gui, "title").getMin(), math::Vec2(0.0F, 0.0F));
    const math::Rect use = getBounds(*gui, "use");
    EXPECT_GE(use.x, 200.0F);
    EXPECT_GE(use.y, 100.0F + getMetric(Theme::Metric::WindowTitleHeight));
    click(use.getCenter());
    EXPECT_EQ(getLastEvent().id + ":" + getLastEvent().name, "use:click");
    EXPECT_TRUE(getUi().isUsingPointer());

    drag({300.0F, 120.0F}, {500.0F, 320.0F});
    EXPECT_EQ(findLastEvent("move").value, (core::Json{{"x", 400.0F}, {"y", 300.0F}}));
    frames();
    EXPECT_EQ(getBounds(*gui, "use").getMin(), use.getMin() + math::Vec2(200.0F, 200.0F));

    // Moves reach the window from the rest of the GUI, and closing the window gives the focus back.
    focus(*gui, "menu");
    key(input::Key::Down);
    key(input::Key::Down);
    EXPECT_TRUE(isFocused(*gui, "use"));
    key(input::Key::Escape);
    frames();
    EXPECT_EQ(findLastEvent("close").id, "bag");
    EXPECT_TRUE(isFocused(*gui, "menu"));
    gui->set("bag", {{"open", true}});
    frames(2);
    const float header = getMetric(Theme::Metric::WindowTitleHeight);
    click({400.0F + 500.0F - header * 0.5F, 300.0F + header * 0.5F});
    EXPECT_EQ(count("close"), 2U);
}

TEST_F(GameComponentTest, MovesAWindowAlongOneAxis) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "window", "id": "map", "title": "Map", "width": 400, "height": 300}]})");
    frames();
    const math::Rect centered = getBounds(*gui, "map");
    gui->set("map", {{"x", 40}});
    frames();
    EXPECT_EQ(getBounds(*gui, "map").getMin(), math::Vec2(40.0F, centered.y));
    gui->set("map", {{"y", 60}});
    frames();
    EXPECT_EQ(getBounds(*gui, "map").getMin(), math::Vec2(40.0F, 60.0F));
}

TEST_F(GameComponentTest, OpensContextMenusWithEveryDevice) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "contextMenu", "id": "menu", "items": [{"id": "rename", "text": "Rename"}, {"id": "delete", "text": "Delete"}], "children": [{"kind": "button", "id": "file", "text": "Save file", "width": 400}]}
    ]})");
    // clang-format on
    const math::Vec2 file = getBounds(*gui, "file").getCenter();
    const float padding = getMetric(Theme::Metric::PanelPadding);
    const float row = getMetric(Theme::Metric::ListRowHeight) * 0.75F;
    pointer(platform::Event::Type::MouseMove, file);
    frames();
    pointer(platform::Event::Type::MouseDown, file, input::MouseButton::Right);
    frames();
    pointer(platform::Event::Type::MouseUp, file, input::MouseButton::Right);
    frames(2);
    click({file.x + padding + 20.0F, file.y + padding + row * 0.5F});
    EXPECT_EQ(findLastEvent("select").value, (core::Json{{"item", "rename"}}));

    focus(*gui, "file");
    key(input::Key::Menu);
    frames();
    key(input::Key::Down);
    key(input::Key::Down);
    key(input::Key::Enter);
    frames();
    EXPECT_EQ(findLastEvent("select").value, (core::Json{{"item", "delete"}}));

    // A long press opens the menu, and the button under the finger does not take it as a tap.
    clearEvents();
    touch(platform::Event::Type::TouchBegan, 1, file);
    frames(40);
    EXPECT_TRUE(getEngine().isBackCaptured());
    touch(platform::Event::Type::TouchEnded, 1, file);
    frames(2);
    EXPECT_EQ(count("click"), 0U);
}

TEST_F(GameComponentTest, OpensAccordionSections) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "accordion", "id": "faq", "width": 600, "items": [{"id": "a", "text": "First"}, {"id": "b", "text": "Second"}], "children": [{"kind": "label", "id": "one", "text": "Answer one"}, {"kind": "label", "id": "two", "text": "Answer two"}]}]})");
    const float header = getMetric(Theme::Metric::ControlHeight);
    const math::Rect faq = getBounds(*gui, "faq");
    EXPECT_EQ(faq.height, header * 2.0F);
    click({faq.x + 100.0F, faq.y + header * 0.5F});
    EXPECT_EQ(findLastEvent("toggle").value, (core::Json{{"item", "a"}, {"expanded", true}}));
    frames();
    const math::Rect one = getBounds(*gui, "one");
    EXPECT_GT(one.y, faq.y + header);

    click({faq.x + 100.0F, one.getBottom() + getMetric(Theme::Metric::ItemSpacing) + header * 0.5F});
    EXPECT_EQ(findLastEvent("toggle").value, (core::Json{{"item", "b"}, {"expanded", true}}));
    frames();
    EXPECT_EQ(getBounds(*gui, "faq").height, header * 2.0F + getBounds(*gui, "two").height + getMetric(Theme::Metric::ItemSpacing) * 2.0F);

    focus(*gui, "faq");
    key(input::Key::Down);
    key(input::Key::Down);
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("toggle").value, (core::Json{{"item", "b"}, {"expanded", false}}));

    // Opening the second section closed the first one, which reported it.
    EXPECT_EQ(count("toggle"), 4U);
}

TEST_F(GameComponentTest, TurnsCarouselPages) {
    auto gui = mount(R"({"kind": "carousel", "id": "pages", "width": 800, "height": 400, "children": [{"kind": "label", "id": "p1", "text": "One"}, {"kind": "label", "id": "p2", "text": "Two"}, {"kind": "label", "id": "p3", "text": "Three"}]})");
    const math::Rect pages = getBounds(*gui, "pages");
    const float indicators = getMetric(Theme::Metric::PageIndicatorSize) * 3.0F;
    const float radius = getMetric(Theme::Metric::ControlHeight) * 0.4F;
    const float middle = pages.y + (pages.height - indicators) * 0.5F;
    click({pages.getRight() - getMetric(Theme::Metric::ItemSpacing) - radius, middle});
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"page", 2}}));
    frames(60);
    EXPECT_NEAR(getBounds(*gui, "p2").x, pages.x, 1.0F);

    drag({pages.x + 600.0F, middle + 100.0F}, {pages.x + 200.0F, middle + 100.0F});
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"page", 3}}));

    focus(*gui, "pages");
    key(input::Key::Left);
    key(input::Key::Left);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"page", 2}}));

    gui->set("pages", {{"interval", 0.25}, {"loop", true}, {"page", 3}});
    frames(20);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"page", 1}}));
}

TEST_F(GameComponentTest, DragsAndCarriesItemsBetweenSlotsAndLists) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "slotGrid", "id": "bag", "align": "start", "columns": 3, "slots": [{"id": "s1", "count": 3}, {"id": "s2"}, {"id": "s3", "enabled": false}]},
        {"kind": "list", "id": "chest", "draggable": true, "items": [{"id": "gold", "text": "Gold"}]}
    ]})");
    // clang-format on
    const float size = getMetric(Theme::Metric::SlotSize);
    const float gap = getMetric(Theme::Metric::ItemSpacing) * 0.5F;
    const math::Rect bag = getBounds(*gui, "bag");
    const math::Vec2 first{bag.x + size * 0.5F, bag.y + size * 0.5F};
    const math::Vec2 second = first + math::Vec2{size + gap, 0.0F};
    EXPECT_EQ(bag.getSize(), math::Vec2(size * 3.0F + gap * 2.0F, size));

    click(first);
    EXPECT_EQ(findLastEvent("select").value, (core::Json{{"item", "s1"}}));
    drag(first, second);
    EXPECT_EQ(findLastEvent("drag").value, (core::Json{{"item", "s1"}}));
    EXPECT_EQ(findLastEvent("drop").value, (core::Json{{"item", "s2"}, {"source", "bag"}, {"sourceItem", "s1"}}));
    drag(getBounds(*gui, "chest").getCenter(), second);
    EXPECT_EQ(findLastEvent("drop").value, (core::Json{{"item", "s2"}, {"source", "chest"}, {"sourceItem", "gold"}}));

    // The keyboard carries the selected slot to the next one, and cancel puts a carried item back without reaching the GUI.
    focus(*gui, "bag");
    key(input::Key::Enter);
    ASSERT_TRUE(getUi().getFocus().getCarried().has_value());
    key(input::Key::Right);
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("drop").value, (core::Json{{"item", "s2"}, {"source", "bag"}, {"sourceItem", "s1"}}));
    EXPECT_FALSE(getUi().getFocus().getCarried().has_value());
    key(input::Key::Enter);
    key(input::Key::Escape);
    frames();
    EXPECT_FALSE(getUi().getFocus().getCarried().has_value());
    EXPECT_EQ(count("cancel"), 0U);

    // An item carried from a node that stops drawing goes back.
    key(input::Key::Enter);
    ASSERT_TRUE(getUi().getFocus().getCarried().has_value());
    gui->set("bag", {{"visible", false}});
    frames(2);
    EXPECT_FALSE(getUi().getFocus().getCarried().has_value());
}

TEST_F(GameComponentTest, CapturesBindingsForRemapping) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "keyCapture", "id": "jump", "value": "key:space"}]})");
    click(gui->find("jump")->getBounds().getCenter());
    key(input::Key::W);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "key:w"}}));

    focus(*gui, "jump");
    key(input::Key::Enter);
    button(input::GamepadButton::South);
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "button:south"}}));

    click(gui->find("jump")->getBounds().getCenter());
    key(input::Key::Escape);
    frames();
    EXPECT_EQ(getLastEvent().name, "cancel");

    gui->set("jump", {{"sources", core::Json::array({"button", "axis"})}});
    click(gui->find("jump")->getBounds().getCenter());
    key(input::Key::A);
    stick({0.9F, 0.0F});
    stick({});
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "axis:leftX+"}}));
    EXPECT_THROW(gui->set("jump", {{"value", "key:nothing"}}), std::invalid_argument);
    EXPECT_EQ(KeyCapture::describe("key:leftShift"), "Left Shift");
    EXPECT_EQ(KeyCapture::describe("axis:leftX+"), "Left X +");
    EXPECT_EQ(KeyCapture::describe("key:keypad0"), "Keypad 0");
    EXPECT_EQ(KeyCapture::describe("key:digit1"), "1");
    EXPECT_EQ(KeyCapture::describe("key:f12"), "F12");
    EXPECT_EQ(KeyCapture::describe("mouse:right"), "Mouse Right");
}

TEST_F(GameComponentTest, SnapsScrolledItemsIntoPlace) {
    std::string buttons;
    for (int index = 0; index < 6; ++index) {
        buttons += (index > 0 ? ", " : "") + std::string(R"({"kind": "button", "id": "b)") + std::to_string(index) + R"(", "text": "Card", "width": 150})";
    }
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "scroll", "id": "shelf", "axis": "horizontal", "snap": true, "width": 400, "height": 120, "align": "start", "scrollbar": false, "children": [{"kind": "row", "gap": 10, "children": [)" + buttons + R"(]}]}]})");
    const math::Rect shelf = getBounds(*gui, "shelf");
    touch(platform::Event::Type::TouchBegan, 1, {shelf.x + 350.0F, shelf.y + 60.0F});
    frames();
    for (int step = 1; step <= 5; ++step) {
        touch(platform::Event::Type::TouchMoved, 1, {shelf.x + 350.0F - 44.0F * static_cast<float>(step), shelf.y + 60.0F});
        frames();
    }
    touch(platform::Event::Type::TouchEnded, 1, {shelf.x + 130.0F, shelf.y + 60.0F});
    frames(60);
    EXPECT_NEAR(getBounds(*gui, "b1").x, shelf.x, 1.0F);
    EXPECT_EQ(count("click"), 0U);
}

TEST_F(GameComponentTest, FocusesTheFirstTreeRowWhenTheSelectedOneIsHidden) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "tree", "id": "files", "selected": "leaf", "items": [{"id": "root", "text": "Root", "children": [{"id": "leaf", "text": "Leaf"}]}]}]})");
    focus(*gui, "files");
    EXPECT_TRUE(isFocused(*gui, "files"));
}

TEST_F(GameComponentTest, PicksComboItemsWithTheKeyboard) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "combo", "id": "size", "width": 300, "items": [{"id": "small", "text": "Small"}, {"id": "large", "text": "Large"}], "selected": "small"}]})");
    focus(*gui, "size");
    key(input::Key::Enter);
    frames();
    key(input::Key::Down);
    key(input::Key::Enter);
    frames();
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "large"}}));
    EXPECT_TRUE(isFocused(*gui, "size"));
}

} // namespace haylen::ui
