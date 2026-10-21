#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/ui/Backend.hpp"
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

// A range that shows its ends leaves at least the value width of the theme for them, so its track lines up with the track of a slider above it.
TEST_F(GameComponentTest, LinesUpTheTrackOfARangeWithTheTrackOfASlider) {
    // clang-format off
    mount(R"({"kind": "column", "style": {"metrics": {"sliderValueWidth": 240}}, "children": [
        {"kind": "slider", "width": 400, "min": 0, "max": 10, "value": 5, "showValue": true},
        {"kind": "rangeSlider", "width": 400, "min": 0, "max": 10, "low": 2, "high": 8, "showValue": true}
    ]})");
    // clang-format on
    frames(2);
    const math::Color groove = getUi().getTheme().getColor(Theme::Color::BorderStrong);
    std::vector<math::Rect> tracks;
    for (const graphics2d::Shape& shape : getUi().getBackend().getShapes()) {
        if (shape.color == groove && shape.bounds.height == getMetric(Theme::Metric::SliderTrackHeight)) {
            tracks.push_back(shape.bounds);
        }
    }
    ASSERT_EQ(tracks.size(), 2U);
    EXPECT_FLOAT_EQ(tracks[0].x, tracks[1].x);
    EXPECT_FLOAT_EQ(tracks[0].width, tracks[1].width);
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

// The keyboard focus of an accordion goes to the first item that can open, so a disabled first item never swallows the keys.
TEST_F(GameComponentTest, FocusesTheFirstAccordionItemThatCanOpen) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "accordion", "id": "faq", "width": 600, "items": [{"id": "a", "text": "First", "enabled": false}, {"id": "b", "text": "Second"}], "children": [{"kind": "label", "text": "Answer one"}, {"kind": "label", "text": "Answer two"}]}]})");
    focus(*gui, "faq");
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("toggle").value, (core::Json{{"item", "b"}, {"expanded", true}}));
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

// The dot of the page that shows lights up, the first one on the first page.
TEST_F(GameComponentTest, LightsTheDotOfTheShownPage) {
    auto gui = mount(R"({"kind": "carousel", "id": "pages", "width": 800, "height": 400, "children": [{"kind": "label", "text": "One"}, {"kind": "label", "text": "Two"}, {"kind": "label", "text": "Three"}]})");
    const float size = getMetric(Theme::Metric::PageIndicatorSize);
    const math::Color accent = getUi().getTheme().getColor(Theme::Color::Accent);
    // clang-format off
    const auto litDot = [&]() -> std::optional<std::size_t> {
        std::vector<std::pair<float, bool>> dots;
        for (const graphics2d::Shape& shape : getUi().getBackend().getShapes()) {
            if (shape.bounds.width == size && shape.bounds.height == size) {
                dots.emplace_back(shape.bounds.x, shape.color == accent);
            }
        }
        std::ranges::sort(dots);
        const auto lit = std::ranges::find_if(dots, [](const auto& dot) { return dot.second; });
        if (dots.size() != 3 || lit == dots.end()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(lit - dots.begin());
    };
    // clang-format on
    frames(2);
    EXPECT_EQ(litDot(), 0U);
    gui->set("pages", {{"page", 3}});
    frames(2);
    EXPECT_EQ(litDot(), 2U);
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
    EXPECT_EQ(findLastEvent("dragStart").value, (core::Json{{"item", "s1"}}));
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

TEST_F(GameComponentTest, ChoosesTheRowsOfADraggableListBeforeCarryingThem) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "list", "id": "queue", "draggable": true, "items": [{"id": "a", "text": "A"}, {"id": "b", "text": "B"}, {"id": "c", "text": "C"}]}]})");
    focus(*gui, "queue");
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("select").value, (core::Json{{"item", "a"}}));
    EXPECT_FALSE(getUi().getFocus().getCarried().has_value());

    // Accept on the chosen row picks it up, and accept on another row drops it there.
    key(input::Key::Enter);
    ASSERT_TRUE(getUi().getFocus().getCarried().has_value());
    EXPECT_EQ(findLastEvent("dragStart").value, (core::Json{{"item", "a"}}));
    key(input::Key::Down);
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("drop").value, (core::Json{{"item", "b"}, {"source", "queue"}, {"sourceItem", "a"}}));
    EXPECT_EQ(count("select"), 1U);
}

TEST_F(GameComponentTest, DrawsAndRegistersOnlyTheRowsOfAListInView) {
    // clang-format off
    const auto targets = [this](int rows) {
        core::Json items = core::Json::array();
        for (int index = 0; index < rows; ++index) {
            items.push_back({{"id", "i" + std::to_string(index)}, {"text", "Row " + std::to_string(index)}});
        }
        auto gui = mount(core::Json{{"kind", "scroll"}, {"id", "box"}, {"height", 300}, {"align", "start"}, {"children", {{{"kind", "list"}, {"id", "list"}, {"draggable", true}, {"items", items}}}}}.dump());
        frames(2);
        const std::size_t count = getUi().getFocus().getTargetCount();
        getUi().unmount(*gui);
        return count;
    };
    // clang-format on
    EXPECT_EQ(targets(20), targets(2000));

    // The focus walks past the rows in view, which come into view as it reaches them.
    core::Json items = core::Json::array();
    for (int index = 0; index < 2000; ++index) {
        items.push_back({{"id", "i" + std::to_string(index)}, {"text", "Row " + std::to_string(index)}});
    }
    auto gui = mount(core::Json{{"kind", "scroll"}, {"height", 300}, {"align", "start"}, {"children", {{{"kind", "list"}, {"id", "list"}, {"items", items}}}}}.dump());
    focus(*gui, "list");
    key(input::Key::Down);
    key(input::Key::Enter);
    const int start = std::stoi(findLastEvent("select").value.at("item").get<std::string>().substr(1));
    for (int step = 0; step < 30; ++step) {
        key(input::Key::Down);
    }
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("select").value, (core::Json{{"item", "i" + std::to_string(start + 30)}}));
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
    frames(20);
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

// A radio group whose selected option is disabled gives the focus to the first option that can be picked.
TEST_F(GameComponentTest, FocusesARadioOptionThatCanBePicked) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "radioGroup", "id": "speed", "items": [{"id": "slow", "text": "Slow", "enabled": false}, {"id": "fast", "text": "Fast"}], "selected": "slow"}]})");
    focus(*gui, "speed");
    key(input::Key::Enter);
    frames();
    EXPECT_EQ(findLastEvent("change").value, (core::Json{{"value", "fast"}}));
}

// A touch button draws its label inside its ring with a margin, smaller than its font when the label is wider than the room, and a short label at its full size.
TEST_F(GameComponentTest, FitsTheLabelOfATouchButtonInsideItsRing) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "children": [
        {"kind": "touchButton", "id": "attack", "action": "attack", "text": "Attack", "size": 100},
        {"kind": "touchButton", "id": "go", "action": "go", "text": "Go", "size": 100}
    ]})");
    // clang-format on
    frames(2);
    Backend& backend = getUi().getBackend();
    const float spacing = getUi().getTheme().getMetric(Theme::Metric::ContentSpacing);
    for (const Backend::Text& text : backend.getTexts()) {
        const std::string_view content = backend.getContent(text);
        const math::Rect button = getBounds(*gui, content == "Go" ? "go" : "attack");
        const std::shared_ptr<const text::Layout> laid = text.family->layout(content, text.style);
        ASSERT_EQ(laid->lines.size(), 1U) << content;
        const math::Rect box = laid->lines.front().box.translated(text.origin);
        EXPECT_GE(box.x, button.x + spacing - 1.0F) << content;
        EXPECT_LE(box.getRight(), button.getRight() - spacing + 1.0F) << content;
    }
}

// The segments of a cramped control take the widths of their labels, so every label keeps a padding from the edges of its segment, while a roomy control shares its width equally.
TEST_F(GameComponentTest, KeepsAPaddingAroundTheLabelsOfSegments) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "segmentedControl", "id": "mode", "width": 440, "selected": "floating", "items": [{"id": "fixed", "text": "Fixed"}, {"id": "floating", "text": "Floating"}, {"id": "following", "text": "Following"}]}]})");
    frames(2);
    Backend& backend = getUi().getBackend();
    const float spacing = getUi().getTheme().getMetric(Theme::Metric::ContentSpacing);
    const math::Rect control = getBounds(*gui, "mode");
    std::vector<math::Rect> boxes;
    for (const Backend::Text& text : backend.getTexts()) {
        const std::shared_ptr<const text::Layout> laid = text.family->layout(backend.getContent(text), text.style);
        boxes.push_back(laid->lines.front().box.translated(text.origin));
        EXPECT_EQ(backend.getContent(text).find("\xE2\x80\xA6"), std::string_view::npos);
    }
    ASSERT_EQ(boxes.size(), 3U);
    EXPECT_GE(boxes[0].x, control.x + spacing);
    EXPECT_GE(boxes[1].x - boxes[0].getRight(), spacing * 2.0F - 1.0F);
    EXPECT_GE(boxes[2].x - boxes[1].getRight(), spacing * 2.0F - 1.0F);
    EXPECT_LE(boxes[2].getRight(), control.getRight() - spacing);

    // The picked segment of a flat control stays inside its border.
    const Theme& theme = getUi().getTheme();
    const float border = theme.getMetric(Theme::Metric::BorderWidth);
    bool picked = false;
    for (const graphics2d::Shape& shape : backend.getShapes()) {
        if (shape.color == theme.getColor(Theme::Color::Accent)) {
            picked = true;
            EXPECT_GE(shape.bounds.y, control.y + border);
            EXPECT_LE(shape.bounds.getBottom(), control.getBottom() - border);
        }
    }
    EXPECT_TRUE(picked);
}

// The labels of a control too narrow for them draw smaller, whole and inside their segments, instead of ending with an ellipsis, and a label of one word never breaks.
TEST_F(GameComponentTest, ShrinksTheLabelsOfACrampedSegmentedControl) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "segmentedControl", "id": "tint", "width": 300, "selected": "gold", "items": [{"id": "off", "text": "Off"}, {"id": "red", "text": "Red"}, {"id": "gold", "text": "Gold"}, {"id": "ghost", "text": "Ghost"}, {"id": "blue", "text": "Blue"}]}]})");
    frames(2);
    Backend& backend = getUi().getBackend();
    const math::Rect control = getBounds(*gui, "tint");
    EXPECT_EQ(control.height, getMetric(Theme::Metric::ControlHeight));
    ASSERT_EQ(backend.getTexts().size(), 5U);
    float right = control.x;
    for (const Backend::Text& text : backend.getTexts()) {
        const std::string_view content = backend.getContent(text);
        EXPECT_EQ(content.find("\xE2\x80\xA6"), std::string_view::npos) << content;
        const std::shared_ptr<const text::Layout> laid = text.family->layout(content, text.style);
        ASSERT_EQ(laid->lines.size(), 1U) << content;
        const math::Rect box = laid->lines.front().box.translated(text.origin);
        EXPECT_GE(box.x, right) << content;
        EXPECT_LE(box.getRight(), control.getRight()) << content;
        EXPECT_LT(text.style.size, getUi().getContext().getEmSize(Theme::Font::Button)) << content;
        right = box.getRight();
    }
}

// Labels that do not fit even at the least scale wrap between their words, and the control grows taller to hold their lines.
TEST_F(GameComponentTest, WrapsTheLabelsOfAVeryNarrowSegmentedControl) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "segmentedControl", "id": "light", "width": 260, "items": [{"id": "dawn", "text": "Early dawn"}, {"id": "noon", "text": "High noon"}, {"id": "dusk", "text": "Late dusk"}]}]})");
    frames(2);
    Backend& backend = getUi().getBackend();
    const math::Rect control = getBounds(*gui, "light");
    EXPECT_GT(control.height, getMetric(Theme::Metric::ControlHeight));
    ASSERT_EQ(backend.getTexts().size(), 3U);
    for (const Backend::Text& text : backend.getTexts()) {
        const std::string_view content = backend.getContent(text);
        EXPECT_EQ(content.find("\xE2\x80\xA6"), std::string_view::npos) << content;
        const std::shared_ptr<const text::Layout> laid = text.family->layout(content, text.style);
        ASSERT_EQ(laid->lines.size(), 2U) << content;
        EXPECT_GE(text.origin.y, control.y) << content;
        EXPECT_LE(text.origin.y + laid->size.y, control.getBottom()) << content;
    }
}

// Chips round into pills by default, and a theme or a style rounds them less with its chip radius.
TEST_F(GameComponentTest, RoundsChipsWithTheChipRadius) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "children": [
        {"kind": "chip", "id": "pill", "text": "Wood"},
        {"kind": "chip", "id": "tag", "text": "Stone", "style": {"metrics": {"chipRadius": 8}}}
    ]})");
    // clang-format on
    frames(2);
    const math::Rect pill = getBounds(*gui, "pill");
    const math::Rect tag = getBounds(*gui, "tag");
    std::vector<float> radii;
    for (const graphics2d::Shape& shape : getUi().getBackend().getShapes()) {
        if (shape.bounds == pill || shape.bounds == tag) {
            radii.push_back(shape.radii[0]);
        }
    }
    ASSERT_EQ(radii.size(), 2U);
    EXPECT_FLOAT_EQ(radii[0], pill.height * 0.5F);
    EXPECT_FLOAT_EQ(radii[1], 8.0F);
}

} // namespace haylen::ui
