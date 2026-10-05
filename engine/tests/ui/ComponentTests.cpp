#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/EmbeddedFiles.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

namespace {

class ComponentTest : public ::testing::Test {
  protected:
    explicit ComponentTest(std::map<std::string, std::string> files = {}) : fixture(std::move(files)) {
        // Clicks move the focus too, and the focus tests check its events, so these tests keep the events of the components alone.
        // clang-format off
        connection = getUi().events.connect([this](Gui&, const Event& event) {
            if (event.name != "focus" && event.name != "blur") {
                events.push_back(event);
            }
        });
        // clang-format on
    }

    [[nodiscard]] static std::string toText(std::span<const std::uint8_t> bytes) {
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }

    core::Engine& getEngine() {
        return fixture.engine();
    }
    plugins::UiPlugin& getUi() {
        return getEngine().getPlugin<plugins::UiPlugin>();
    }

    std::shared_ptr<Gui> mount(const std::string& json) {
        auto gui = getUi().createGui(core::Json::parse(json), Placement::Screen);
        getUi().mount(gui);
        frames();
        return gui;
    }

    void frames(int count = 1) {
        fixture.frames(count);
    }

    void pointer(platform::Event::Type type, math::Vec2 point) {
        platform::Event event;
        event.type = type;
        event.position = getEngine().getViewport().toFramebuffer(point + getEngine().getViewport().getVisibleRect().getMin());
        getEngine().handleEvent(event);
    }

    void key(input::Key code) {
        for (const platform::Event::Type type : {platform::Event::Type::KeyDown, platform::Event::Type::KeyUp}) {
            platform::Event event;
            event.type = type;
            event.key = code;
            getEngine().handleEvent(event);
            frames();
        }
    }

    void type(std::u32string_view text) {
        for (const char32_t character : text) {
            platform::Event event;
            event.type = platform::Event::Type::Character;
            event.character = character;
            getEngine().handleEvent(event);
            frames();
        }
    }

    void touch(platform::Event::Type type, std::uint64_t id, math::Vec2 point) {
        platform::Event event;
        event.type = type;
        event.touchCount = 1;
        event.touches[0] = {.id = id, .position = getEngine().getViewport().toFramebuffer(point + getEngine().getViewport().getVisibleRect().getMin()), .changed = true};
        getEngine().handleEvent(event);
    }

    // Presses and releases at a point, then runs the frame that delivers the resulting events.
    void click(math::Vec2 point) {
        pointer(platform::Event::Type::MouseMove, point);
        frames();
        pointer(platform::Event::Type::MouseDown, point);
        frames();
        pointer(platform::Event::Type::MouseUp, point);
        frames(2);
    }

    void click(const Gui& gui, const std::string& id) {
        click(gui.find(id)->getBounds().getCenter());
    }

    [[nodiscard]] const math::Rect& getBounds(const Gui& gui, const std::string& id) const {
        return gui.find(id)->getBounds();
    }

    [[nodiscard]] std::vector<std::string> getEventNames() const {
        std::vector<std::string> list;
        for (const Event& event : events) {
            list.push_back(event.id + ":" + event.name);
        }
        return list;
    }

    [[nodiscard]] const Event& getLastEvent() const {
        return events.back();
    }

    // Runs a frame and counts the vertices the GUI window drew, where every glyph adds four.
    // Counts what the GUIs draw: the ImGui vertices of their window and the glyphs their text draws through the renderer.
    [[nodiscard]] int countDrawn() {
        frames();
        getUi().getBackend().makeCurrent();
        return ImGui::FindWindowByName("##haylen-guis")->DrawList->VtxBuffer.Size + static_cast<int>(getEngine().getRenderer2D().getStats().instances);
    }

    test::EngineFixture fixture;
    std::vector<Event> events;
    core::Connection connection;
};

} // namespace

TEST_F(ComponentTest, DrawsEveryKind) {
    // clang-format off
    mount(R"({"kind": "scroll", "height": 1080, "children": [{"kind": "column", "padding": 16, "children": [
        {"kind": "pageHeader", "title": "Settings", "caption": "Tune the island", "banner": true},
        {"kind": "sectionTitle", "text": "Audio"},
        {"kind": "alert", "tone": "warning", "title": "Careful", "message": "Night is coming"},
        {"kind": "emptyState", "title": "Nothing here", "message": "Chop some trees"},
        {"kind": "label", "text": "A long line of text that wraps across the width of the column when it is narrow enough", "textAlign": "center", "outline": "#FF000000"},
        {"kind": "label", "text": "Single line", "wrap": false, "textAlign": "end"},
        {"kind": "row", "justify": "center", "children": [
            {"kind": "button", "text": "Default"}, {"kind": "button", "text": "Primary", "variant": "primary"}, {"kind": "button", "text": "Delete", "variant": "destructive"},
            {"kind": "button", "text": "Tool", "variant": "toolbar", "checked": true}, {"kind": "button", "variant": "icon"}, {"kind": "button", "text": "Link", "variant": "link"}
        ]},
        {"kind": "grid", "columns": 3, "children": [{"kind": "badge", "text": "New", "tone": "accent", "solid": true}, {"kind": "statusIndicator", "text": "Online"}, {"kind": "busyIndicator"}, {"kind": "avatar", "name": "Ana Souza"}]},
        {"kind": "stack", "height": 80, "children": [{"kind": "panel"}, {"kind": "label", "text": "Over", "align": "center"}]},
        {"kind": "card", "children": [{"kind": "progress", "value": 0.4, "text": "40%"}, {"kind": "divider"}, {"kind": "divider", "vertical": true, "height": 20}]},
        {"kind": "formField", "label": "Name", "required": true, "help": "Shown on the board", "children": [{"kind": "textField", "placeholder": "Your name"}]},
        {"kind": "formField", "label": "Password", "error": "Too short", "children": [{"kind": "secretField", "value": "123"}]},
        {"kind": "textArea", "rows": 3, "value": "line one\nline two"},
        {"kind": "filterField", "value": "wood"},
        {"kind": "numberField", "value": 3, "min": 0, "max": 9, "decimals": 1},
        {"kind": "slider", "value": 0.3, "showValue": true},
        {"kind": "colorField", "value": "#FF2E7D32"},
        {"kind": "combo", "items": [{"id": "a", "text": "Alpha"}], "placeholder": "Pick one"},
        {"kind": "radioGroup", "horizontal": true, "items": [{"id": "a", "text": "A"}, {"id": "b", "text": "B", "enabled": false}], "selected": "a"},
        {"kind": "checkbox", "text": "Check", "checked": true},
        {"kind": "toggle", "text": "Toggle"},
        {"kind": "chip", "text": "Chip", "selected": true},
        {"kind": "menuButton", "text": "Menu", "items": [{"id": "x", "text": "X"}]},
        {"kind": "popover", "text": "More", "children": [{"kind": "label", "text": "Inside"}]},
        {"kind": "tabs", "items": [{"id": "one", "text": "One"}, {"id": "two", "text": "Two"}], "children": [{"kind": "label", "text": "First"}, {"kind": "label", "text": "Second"}]},
        {"kind": "list", "items": [{"id": "a", "text": "Alpha", "caption": "First letter"}, {"id": "b", "text": "Beta", "enabled": false}], "selected": "a"},
        {"kind": "tree", "items": [{"id": "root", "text": "Root", "children": [{"id": "leaf", "text": "Leaf"}]}], "expanded": ["root"]},
        {"kind": "table", "columns": [{"text": "Name"}, {"text": "Score", "width": 120, "align": "end"}], "rows": [{"id": "r1", "cells": ["Ana", 12]}]},
        {"kind": "settingsForm", "children": [
            {"kind": "sectionTitle", "text": "Video"},
            {"kind": "settingsRow", "label": "Fullscreen", "caption": "Use the whole screen", "children": [{"kind": "toggle"}]},
            {"kind": "settingsActions", "children": [{"kind": "button", "text": "Cancel"}, {"kind": "button", "text": "Save", "variant": "primary"}]}
        ]},
        {"kind": "splitter", "height": 100, "children": [{"kind": "label", "text": "Left"}, {"kind": "label", "text": "Right"}]},
        {"kind": "safeArea", "children": [{"kind": "spacer", "height": 10}]},
        {"kind": "touchStick", "action": "move", "radius": 60},
        {"kind": "touchButton", "action": "jump", "text": "A", "size": 80},
        {"kind": "toast", "open": true, "text": "Saved", "tone": "success", "position": "bottom"},
        {"kind": "dialog", "open": true, "title": "Quit?", "message": "Progress is saved.", "buttons": [{"id": "no", "text": "Stay"}, {"id": "yes", "text": "Quit", "variant": "destructive"}], "children": [{"kind": "label", "text": "Really"}]}
    ]}]})");
    // clang-format on
    frames(3);
    EXPECT_EQ(getEngine().getError(), nullptr) << getEngine().getError()->what();
    EXPECT_GT(getEngine().getRenderer2D().getStats().vertices, 0U);
}

TEST_F(ComponentTest, WrapsTextTheSameWayAtItsMeasuredWidth) {
    frames();
    const std::string text = "Thrusts push enemies away. Special charges ahead, and every word must stay on its measured line.";
    for (const Theme::Font font : {Theme::Font::Body, Theme::Font::Caption, Theme::Font::Heading}) {
        for (float width = 120.0F; width < 900.0F; width += 7.0F) {
            const math::Vec2 measured = Typography::measureParagraph(getUi().getContext(), font, text, width);
            ui::Context& context = getUi().getContext();
            EXPECT_EQ(Typography::layout(context, font, text, Typography::getStyle(context, font, measured.x))->lines.size(), Typography::layout(context, font, text, Typography::getStyle(context, font, width))->lines.size()) << width;
        }
    }
}

TEST_F(ComponentTest, ReportsButtonsAndChoices) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "gap": 12, "padding": 20, "children": [
        {"kind": "button", "id": "play", "text": "Play", "variant": "primary"},
        {"kind": "checkbox", "id": "music", "text": "Music"},
        {"kind": "toggle", "id": "sound", "text": "Sound", "checked": true},
        {"kind": "radioGroup", "id": "level", "items": [{"id": "easy", "text": "Easy"}, {"id": "hard", "text": "Hard"}], "selected": "easy"},
        {"kind": "chip", "id": "tag", "text": "Wood", "removable": true}
    ]})");
    // clang-format on

    click(*gui, "play");
    click(*gui, "music");
    EXPECT_EQ(getLastEvent().value, (core::Json{{"checked", true}}));
    click(*gui, "sound");
    EXPECT_EQ(getLastEvent().value, (core::Json{{"checked", false}}));

    const math::Rect level = getBounds(*gui, "level");
    click({level.x + 20.0F, level.y + level.height * 0.75F});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"value", "hard"}}));
    click({level.x + 20.0F, level.y + level.height * 0.75F});

    const math::Rect tag = getBounds(*gui, "tag");
    click({tag.x + 10.0F, tag.getCenter().y});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"selected", true}}));
    click({tag.getRight() - 6.0F, tag.getCenter().y});
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"play:click", "music:change", "sound:change", "level:change", "tag:change", "tag:remove"}));
}

TEST_F(ComponentTest, DragsSlidersAndTypesText) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "slider", "id": "volume", "width": 400, "min": 0, "max": 1},
        {"kind": "slider", "id": "stepped", "width": 400, "min": 0, "max": 10, "step": 5},
        {"kind": "textField", "id": "name", "maxLength": 4},
        {"kind": "numberField", "id": "count", "value": 2, "min": 0, "max": 3, "step": 2},
        {"kind": "filterField", "id": "search"}
    ]})");
    // clang-format on

    const math::Rect volume = getBounds(*gui, "volume");
    pointer(platform::Event::Type::MouseMove, {volume.x + 20.0F, volume.getCenter().y});
    frames();
    pointer(platform::Event::Type::MouseDown, {volume.x + 20.0F, volume.getCenter().y});
    frames();
    pointer(platform::Event::Type::MouseMove, {volume.x + volume.width * 0.75F, volume.getCenter().y});
    frames();
    pointer(platform::Event::Type::MouseUp, {volume.x + volume.width * 0.75F, volume.getCenter().y});
    frames(2);
    ASSERT_FALSE(events.empty());
    const double dragged = getLastEvent().value.at("value").get<double>();
    EXPECT_GT(dragged, 0.6);
    EXPECT_LT(dragged, 0.9);

    const math::Rect stepped = getBounds(*gui, "stepped");
    click({stepped.x + stepped.width * 0.6F, stepped.getCenter().y});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"value", 5.0}}));

    click(*gui, "name");
    events.clear();
    type(U"abcdef");
    key(input::Key::Enter);
    frames();
    EXPECT_EQ(getLastEvent().name, "submit");
    EXPECT_EQ(getLastEvent().value, (core::Json{{"value", "abcd"}}));

    const math::Rect count = getBounds(*gui, "count");
    click({count.getRight() - count.height * 0.5F, count.getCenter().y});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"value", 3.0}}));
    click({count.x + count.height * 0.5F, count.getCenter().y});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"value", 1.0}}));

    gui->command(getUi().getContext(), "search", "focus", core::Json::object());
    frames(2);
    type(U"oak");
    const math::Rect search = getBounds(*gui, "search");
    click({search.getRight() - search.height * 0.3F, search.getCenter().y});
    EXPECT_EQ(getLastEvent().id, "search");
    EXPECT_EQ(getLastEvent().value, (core::Json{{"value", ""}}));
    EXPECT_THROW(gui->command(getUi().getContext(), "count", "shake", core::Json::object()), std::invalid_argument);
}

TEST_F(ComponentTest, SelectsInContainersAndCollections) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "tabs", "id": "tabs", "items": [{"id": "one", "text": "Same"}, {"id": "two", "text": "Same"}], "children": [{"kind": "label", "text": "1"}, {"kind": "label", "text": "2"}]},
        {"kind": "list", "id": "list", "items": [{"id": "a", "text": "Alpha"}, {"id": "b", "text": "Beta"}]},
        {"kind": "tree", "id": "tree", "items": [{"id": "root", "text": "Root", "children": [{"id": "leaf", "text": "Leaf"}]}]},
        {"kind": "table", "id": "table", "columns": [{"text": "Name"}], "rows": [{"id": "r1", "cells": ["Ana"]}, {"id": "r2", "cells": ["Bia"]}]},
        {"kind": "splitter", "id": "split", "height": 60, "children": [{"kind": "spacer"}, {"kind": "spacer"}]}
    ]})");
    // clang-format on
    const float row = getUi().getTheme().getMetric(Theme::Metric::ListRowHeight);

    const math::Rect tabs = getBounds(*gui, "tabs");
    getUi().getBackend().makeCurrent();
    const float tab = Typography::measure(getUi().getContext(), Theme::Font::Button, "Same").x + getUi().getTheme().getMetric(Theme::Metric::TabPaddingX) * 2.0F;
    click({tabs.x + tab * 1.5F, tabs.y + 10.0F});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"item", "two"}}));

    const math::Rect list = getBounds(*gui, "list");
    click({list.getCenter().x, list.y + row * 1.5F});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"item", "b"}}));

    math::Rect tree = getBounds(*gui, "tree");
    click({tree.x + getUi().getTheme().getMetric(Theme::Metric::IconSize) * 0.5F, tree.y + row * 0.5F});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"item", "root"}, {"expanded", true}}));
    tree = getBounds(*gui, "tree");
    click({tree.getCenter().x, tree.y + row * 1.5F});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"item", "leaf"}}));

    const math::Rect table = getBounds(*gui, "table");
    click({table.getCenter().x, table.y + row * 0.75F + row * 1.5F});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"item", "r2"}}));

    const math::Rect split = getBounds(*gui, "split");
    pointer(platform::Event::Type::MouseMove, split.getCenter());
    frames();
    pointer(platform::Event::Type::MouseDown, split.getCenter());
    frames();
    pointer(platform::Event::Type::MouseMove, {split.x + split.width * 0.25F, split.getCenter().y});
    frames();
    pointer(platform::Event::Type::MouseUp, {split.x + split.width * 0.25F, split.getCenter().y});
    frames(2);
    EXPECT_EQ(getLastEvent().name, "resize");
    EXPECT_LT(getLastEvent().value.at("ratio").get<double>(), 0.4);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"tabs:select", "list:select", "tree:toggle", "tree:select", "table:select", "split:resize"}));

    for (const double width : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), 1e300}) {
        core::Json columns = core::Json::array();
        columns.push_back({{"width", width}});
        EXPECT_THROW(gui->set("table", {{"columns", columns}}), std::invalid_argument) << width;
    }
}

TEST_F(ComponentTest, RunsDialogsToastsAndPopups) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "dialog", "id": "quit", "open": true, "title": "Quit?", "buttons": [{"id": "no", "text": "Stay"}, {"id": "yes", "text": "Quit"}]},
        {"kind": "toast", "id": "saved", "text": "Saved", "duration": 0.5},
        {"kind": "combo", "id": "size", "width": 300, "items": [{"id": "small", "text": "Small"}, {"id": "large", "text": "Large"}]},
        {"kind": "menuButton", "id": "menu", "text": "Menu", "items": [{"id": "save", "text": "Save"}]}
    ]})");
    // clang-format on
    frames(2);
    key(input::Key::Enter);
    frames(2);
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(getLastEvent().id, "quit");
    EXPECT_EQ(getLastEvent().value, (core::Json{{"button", "yes"}}));

    gui->set("quit", {{"open", true}});
    frames(2);
    key(input::Key::Escape);
    frames(2);
    EXPECT_EQ(getLastEvent().name, "dismiss");

    gui->set("saved", {{"open", true}});
    frames(45);
    EXPECT_EQ(getLastEvent().id, "saved");
    EXPECT_EQ(getLastEvent().name, "dismiss");

    const float padding = getUi().getTheme().getMetric(Theme::Metric::MenuPadding);
    const float item = getUi().getTheme().getMetric(Theme::Metric::ListRowHeight) * 0.75F;
    const math::Rect size = getBounds(*gui, "size");
    click(size.getCenter());
    click({size.x + padding + 20.0F, size.getBottom() + 4.0F + padding + item * 0.5F});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"value", "small"}}));

    const math::Rect menu = getBounds(*gui, "menu");
    click(menu.getCenter());
    click({menu.x + padding + 20.0F, menu.getBottom() + 4.0F + padding + item * 0.5F});
    EXPECT_EQ(getLastEvent().value, (core::Json{{"item", "save"}}));
}

// A dialog or a menu whose node stops drawing closes, so it no longer keeps the pointer and cancel from the rest of the UI.
TEST_F(ComponentTest, ClosesThePopupsOfNodesThatStopDrawing) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "play", "text": "Play"},
        {"kind": "menuButton", "id": "menu", "text": "Menu", "items": [{"id": "save", "text": "Save"}]},
        {"kind": "dialog", "id": "quit", "open": true, "title": "Quit?", "buttons": [{"id": "stay", "text": "Stay"}]}
    ]})");
    // clang-format on
    frames(2);
    ASSERT_TRUE(getUi().isCapturingBack());
    gui->set("quit", {{"visible", false}});
    frames(2);
    EXPECT_FALSE(getUi().isCapturingBack());
    click(*gui, "play");
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"play:click"}));

    click(*gui, "menu");
    ASSERT_TRUE(getUi().isCapturingBack());
    gui->set("menu", {{"visible", false}});
    frames(2);
    EXPECT_FALSE(getUi().isCapturingBack());

    gui->set("quit", {{"visible", true}});
    frames(2);
    ASSERT_TRUE(getUi().isCapturingBack());
    gui->setVisible(false);
    frames(2);
    EXPECT_FALSE(getUi().isCapturingBack());

    gui->setVisible(true);
    frames(2);
    ASSERT_TRUE(getUi().isCapturingBack());
    getUi().unmount(*gui);
    frames(2);
    EXPECT_FALSE(getUi().isCapturingBack());
}

TEST_F(ComponentTest, ShowsOneDialogAtATime) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "dialog", "id": "first", "open": true, "title": "First", "buttons": [{"id": "ok", "text": "OK"}]},
        {"kind": "dialog", "id": "second", "open": true, "title": "Second", "buttons": [{"id": "ok", "text": "OK"}]}
    ]})");
    // clang-format on
    frames(2);
    key(input::Key::Enter);

    // The second dialog waits until the first one faded out.
    frames(12);
    key(input::Key::Enter);
    frames(2);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"first:answer", "second:answer"}));
}

TEST_F(ComponentTest, ScrollsTheContentOfDialogsTallerThanTheScreen) {
    std::string buttons;
    for (int index = 0; index < 30; ++index) {
        buttons += std::string(index == 0 ? "" : ", ") + R"({"kind": "button", "id": "b)" + std::to_string(index) + R"(", "text": "Rule"})";
    }
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "dialog", "id": "rules", "open": true, "title": "Rules", "buttons": [{"id": "ok", "text": "OK"}], "children": [)" + buttons + "]}]}");
    frames(2);
    const math::Rect dialog = getBounds(*gui, "rules");
    EXPECT_LE(dialog.getBottom(), 1080.0F);
    EXPECT_GT(getBounds(*gui, "b29").getBottom(), dialog.getBottom());

    gui->command(getUi().getContext(), "b29", "focus", core::Json::object());
    frames(3);
    EXPECT_LE(getBounds(*gui, "b29").getBottom(), dialog.getBottom());
}

// ImGui gives a child window without a size the rest of its window, which an empty scroll must not take.
TEST_F(ComponentTest, LetsThePointerThroughAnEmptyScroll) {
    auto gui = mount(R"({"kind": "stack", "children": [{"kind": "button", "id": "play", "text": "Play", "align": "stretch"}, {"kind": "scroll", "align": "start"}]})");
    click(*gui, "play");
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"play:click"}));
}

// Typing reports only numbers inside the range, and the end of the editing brings any other number into it. Text that is not a finite number changes nothing.
TEST_F(ComponentTest, CommitsTypedNumbersInsideTheRange) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "numberField", "id": "count", "value": 20, "min": 10, "max": 100}]})");
    // clang-format off
    const auto retype = [this, &gui](std::u32string_view text) {
        click(*gui, "count");
        key(input::Key::Backspace);
        key(input::Key::Backspace);
        type(text);
    };
    // clang-format on
    retype(U"50");
    key(input::Key::Enter);
    retype(U"5");
    key(input::Key::Enter);
    retype(U"nan");
    key(input::Key::Enter);
    frames(3);
    ASSERT_EQ(getEventNames(), (std::vector<std::string>{"count:change", "count:change"}));
    EXPECT_EQ(events[0].value, (core::Json{{"value", 50.0}}));
    EXPECT_EQ(events[1].value, (core::Json{{"value", 10.0}}));
}

TEST_F(ComponentTest, MeasuresGridsWithTheGapsBetweenTheirColumns) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "grid", "id": "grid", "columns": 3, "gap": 20, "align": "start", "children": [
        {"kind": "spacer", "width": 100, "height": 10}, {"kind": "spacer", "width": 100, "height": 10}, {"kind": "spacer", "width": 100, "height": 10}
    ]}]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "grid").width, 340.0F);
}

// A horizontal scroll offers its child an unbounded width, where kinds that fill the width they get take the width of their content instead.
TEST_F(ComponentTest, GivesKindsThatFillTheirWidthAFiniteWidthInAHorizontalScroll) {
    // clang-format off
    auto gui = mount(R"({"kind": "scroll", "axis": "horizontal", "height": 800, "children": [{"kind": "column", "id": "content", "children": [
        {"kind": "list", "id": "list", "items": [{"id": "a", "text": "Alpha"}]},
        {"kind": "tree", "items": [{"id": "root", "text": "Root", "children": [{"id": "leaf", "text": "Leaf"}]}], "expanded": ["root"]},
        {"kind": "table", "columns": [{"text": "Name"}, {"text": "Score", "width": 120}], "rows": [{"id": "r1", "cells": ["Ana", 12]}]},
        {"kind": "accordion", "items": [{"id": "a", "text": "First"}], "expanded": ["a"], "children": [{"kind": "label", "text": "Answer"}]},
        {"kind": "carousel", "height": 100, "children": [{"kind": "label", "text": "Page"}]},
        {"kind": "grid", "columns": 2, "children": [{"kind": "list", "items": [{"id": "b", "text": "Beta"}]}, {"kind": "label", "text": "Cell"}]},
        {"kind": "splitter", "height": 60, "children": [{"kind": "list", "items": [{"id": "c", "text": "Gamma"}]}, {"kind": "label", "text": "Side"}]}
    ]}]})");
    // clang-format on
    const float width = getBounds(*gui, "content").width;
    EXPECT_GT(width, 0.0F);
    EXPECT_LT(width, 1000.0F);
    EXPECT_EQ(getBounds(*gui, "list").width, width);
}

// A splitter measures its children at the shares of its width they draw at, and a stacked splitter makes room for both.
TEST_F(ComponentTest, MeasuresSplitterChildrenAtTheirShares) {
    const std::string text = "A long line of text that wraps across the half of the splitter it gets, several times over, before the other half starts.";
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "splitter", "id": "split", "children": [{"kind": "label", "id": "left", "text": ")" + text + R"("}, {"kind": "label", "text": "Short"}]},
        {"kind": "splitter", "vertical": true, "children": [{"kind": "label", "id": "top", "text": "Top"}, {"kind": "label", "id": "bottom", "text": "Bottom"}]}
    ]})");
    // clang-format on
    frames();
    Context& context = getUi().getContext();
    const float line = std::floor(Typography::getLineHeight(context, Theme::Font::Body));
    EXPECT_GE(getBounds(*gui, "split").height, Typography::measureParagraph(context, Theme::Font::Body, text, getBounds(*gui, "left").width).y);
    EXPECT_GE(getBounds(*gui, "top").height, line);
    EXPECT_GE(getBounds(*gui, "bottom").height, line);
}

// Only the rows in view draw their content, while every row still takes the focus and the pointer.
TEST_F(ComponentTest, DrawsOnlyTheRowsOfAScrolledListInView) {
    // clang-format off
    const auto count = [this](int rows) {
        std::string items;
        for (int index = 0; index < rows; ++index) {
            items += std::string(index == 0 ? "" : ", ") + R"({"id": "i)" + std::to_string(index) + R"(", "text": "Row )" + std::to_string(index) + R"("})";
        }
        auto gui = mount(R"({"kind": "scroll", "height": 300, "align": "start", "children": [{"kind": "list", "id": "list", "items": [)" + items + "]}]}");
        const int drawn = countDrawn();
        getUi().unmount(*gui);
        return drawn;
    };
    // clang-format on
    EXPECT_EQ(count(20), count(200));
}

TEST_F(ComponentTest, TouchControlsIgnoreFingersWhileDisabled) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "padding": 40, "gap": 400, "children": [
        {"kind": "touchStick", "id": "stick", "action": "move", "radius": 100, "deadZone": 0},
        {"kind": "touchButton", "id": "attack", "action": "attack", "size": 120}
    ]})");
    // clang-format on
    const math::Rect stick = getBounds(*gui, "stick");
    const math::Rect attack = getBounds(*gui, "attack");
    input::VirtualInput& controls = getEngine().getVirtualInput();
    touch(platform::Event::Type::TouchBegan, 1, stick.getCenter() + math::Vec2{50.0F, 0.0F});
    touch(platform::Event::Type::TouchBegan, 2, attack.getCenter());
    frames(2);
    ASSERT_TRUE(controls.isButtonDown("attack"));

    gui->set("stick", {{"enabled", false}});
    gui->set("attack", {{"enabled", false}});
    frames(2);
    EXPECT_EQ(controls.getStick("move"), math::Vec2());
    EXPECT_FALSE(controls.isButtonDown("attack"));

    // Enabled again under the same fingers, the controls wait for new presses.
    gui->set("stick", {{"enabled", true}});
    gui->set("attack", {{"enabled", true}});
    frames(2);
    EXPECT_EQ(controls.getStick("move"), math::Vec2());
    EXPECT_FALSE(controls.isButtonDown("attack"));
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"attack:press", "attack:release"}));
}

TEST_F(ComponentTest, DrivesVirtualControlsWithSeveralFingers) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "padding": 40, "gap": 400, "children": [
        {"kind": "touchStick", "id": "stick", "action": "move", "radius": 100, "deadZone": 0},
        {"kind": "touchButton", "id": "attack", "action": "attack", "size": 120}
    ]})");
    // clang-format on
    const math::Rect stick = getBounds(*gui, "stick");
    const math::Rect attack = getBounds(*gui, "attack");
    input::VirtualInput& controls = getEngine().getVirtualInput();

    touch(platform::Event::Type::TouchBegan, 1, stick.getCenter() + math::Vec2{50.0F, 0.0F});
    touch(platform::Event::Type::TouchBegan, 2, attack.getCenter());
    frames(2);
    EXPECT_NEAR(controls.getStick("move").x, 0.5F, 0.01F);
    EXPECT_TRUE(controls.isButtonDown("attack"));
    EXPECT_TRUE(getUi().isUsingPointer());

    touch(platform::Event::Type::TouchMoved, 1, stick.getCenter() + math::Vec2{0.0F, -300.0F});
    frames(2);
    EXPECT_NEAR(controls.getStick("move").y, -1.0F, 0.01F);

    touch(platform::Event::Type::TouchEnded, 1, stick.getCenter());
    touch(platform::Event::Type::TouchEnded, 2, attack.getCenter());
    frames(2);
    EXPECT_EQ(controls.getStick("move"), math::Vec2());
    EXPECT_FALSE(controls.isButtonDown("attack"));
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"attack:press", "attack:release"}));

    // A control that stops drawing lets go of what it held.
    click(attack.getCenter());
    touch(platform::Event::Type::TouchBegan, 3, attack.getCenter());
    frames(2);
    EXPECT_TRUE(controls.isButtonDown("attack"));
    gui->set("attack", {{"visible", false}});
    frames(2);
    EXPECT_FALSE(controls.isButtonDown("attack"));
}

TEST_F(ComponentTest, PlacesTheRingOfAStickByItsMode) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "padding": 40, "gap": 100, "children": [
        {"kind": "touchStick", "id": "fixed", "action": "walk", "radius": 50, "deadZone": 0, "width": 400, "height": 400},
        {"kind": "touchStick", "id": "floating", "action": "aim", "mode": "floating", "radius": 50, "deadZone": 0, "width": 400, "height": 400},
        {"kind": "touchStick", "id": "following", "action": "look", "mode": "following", "radius": 50, "deadZone": 0, "width": 400, "height": 400}
    ]})");
    // clang-format on
    input::VirtualInput& controls = getEngine().getVirtualInput();
    const math::Vec2 start{100.0F, 200.0F};

    // Three fingers land at the same spot of each area and move by the same amount, so each mode shows in the vector it reports.
    for (const auto& [finger, id] : {std::pair{1U, "fixed"}, std::pair{2U, "floating"}, std::pair{3U, "following"}}) {
        touch(platform::Event::Type::TouchBegan, finger, getBounds(*gui, id).getMin() + start);
    }
    frames(2);
    EXPECT_NEAR(controls.getStick("walk").x, -1.0F, 0.01F);
    EXPECT_EQ(controls.getStick("aim"), math::Vec2());
    EXPECT_EQ(controls.getStick("look"), math::Vec2());

    // A finger far past the ring pulls a following ring after it, so moving back reads from the new center.
    for (const auto& [finger, id] : {std::pair{2U, "floating"}, std::pair{3U, "following"}}) {
        touch(platform::Event::Type::TouchMoved, finger, getBounds(*gui, id).getMin() + start + math::Vec2{150.0F, 0.0F});
    }
    frames(2);
    EXPECT_NEAR(controls.getStick("aim").x, 1.0F, 0.01F);
    EXPECT_NEAR(controls.getStick("look").x, 1.0F, 0.01F);
    for (const auto& [finger, id] : {std::pair{2U, "floating"}, std::pair{3U, "following"}}) {
        touch(platform::Event::Type::TouchMoved, finger, getBounds(*gui, id).getMin() + start + math::Vec2{100.0F, 0.0F});
    }
    frames(2);
    EXPECT_NEAR(controls.getStick("aim").x, 1.0F, 0.01F);
    EXPECT_NEAR(controls.getStick("look").x, 0.0F, 0.01F);

    // A following ring stays inside the area of its stick.
    touch(platform::Event::Type::TouchMoved, 3, getBounds(*gui, "following").getMin() + math::Vec2{900.0F, 200.0F});
    frames(2);
    touch(platform::Event::Type::TouchMoved, 3, getBounds(*gui, "following").getMin() + math::Vec2{400.0F, 200.0F});
    frames(2);
    EXPECT_NEAR(controls.getStick("look").x, 0.0F, 0.01F);
    EXPECT_THROW(gui->set("fixed", {{"mode", "drifting"}}), std::invalid_argument);
}

TEST_F(ComponentTest, TouchControlsLetGoWhenTheyStopDrawing) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "padding": 40, "gap": 400, "children": [
        {"kind": "touchStick", "id": "stick", "action": "move", "radius": 100, "deadZone": 0},
        {"kind": "touchButton", "id": "attack", "action": "attack", "size": 120}
    ]})");
    // clang-format on
    const math::Rect stick = getBounds(*gui, "stick");
    const math::Rect attack = getBounds(*gui, "attack");
    input::VirtualInput& controls = getEngine().getVirtualInput();

    touch(platform::Event::Type::TouchBegan, 1, stick.getCenter() + math::Vec2{50.0F, 0.0F});
    touch(platform::Event::Type::TouchBegan, 2, attack.getCenter());
    frames(2);
    ASSERT_TRUE(controls.isButtonDown("attack"));
    gui->set("stick", {{"visible", false}});
    gui->set("attack", {{"visible", false}});
    frames(2);
    EXPECT_EQ(controls.getStick("move"), math::Vec2());
    EXPECT_FALSE(controls.isButtonDown("attack"));
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"attack:press", "attack:release"}));

    // Shown again under the same fingers, the controls wait for new presses, and lifting the old fingers reports nothing.
    gui->set("stick", {{"visible", true}});
    gui->set("attack", {{"visible", true}});
    frames(2);
    EXPECT_EQ(controls.getStick("move"), math::Vec2());
    EXPECT_FALSE(controls.isButtonDown("attack"));
    touch(platform::Event::Type::TouchEnded, 1, stick.getCenter());
    touch(platform::Event::Type::TouchEnded, 2, attack.getCenter());
    frames(2);
    EXPECT_EQ(getEventNames().size(), 2U);

    touch(platform::Event::Type::TouchBegan, 3, attack.getCenter());
    frames(2);
    gui->setVisible(false);
    frames(2);
    EXPECT_FALSE(controls.isButtonDown("attack"));
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"attack:press", "attack:release", "attack:press", "attack:release"}));
}

TEST_F(ComponentTest, KeepsMouseClicksOnTheInterfaceFromActions) {
    getEngine().getActions().load(core::Json::parse(R"({"actions": [{"name": "attack", "type": "button", "bindings": ["mouse:left", "virtual:attack"]}]})"));
    auto gui = mount(R"({"kind": "stack", "children": [{"kind": "button", "id": "pause", "text": "Pause", "align": "start"}]})");
    const math::Vec2 button = getBounds(*gui, "pause").getCenter();

    pointer(platform::Event::Type::MouseMove, button);
    frames(2);
    pointer(platform::Event::Type::MouseDown, button);
    frames();
    EXPECT_TRUE(getEngine().getInput().isPointerCaptured());
    EXPECT_FALSE(getEngine().getActions().isDown("attack"));
    pointer(platform::Event::Type::MouseUp, button);
    frames(2);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"pause:click"}));

    pointer(platform::Event::Type::MouseMove, {1500.0F, 900.0F});
    frames(2);
    pointer(platform::Event::Type::MouseDown, {1500.0F, 900.0F});
    frames();
    EXPECT_FALSE(getEngine().getInput().isPointerCaptured());
    EXPECT_TRUE(getEngine().getActions().isDown("attack"));
    pointer(platform::Event::Type::MouseUp, {1500.0F, 900.0F});
    frames();

    // Virtual buttons keep driving actions while the interface owns the pointer.
    pointer(platform::Event::Type::MouseMove, button);
    frames(2);
    getEngine().getVirtualInput().setButton("attack", true);
    frames();
    EXPECT_TRUE(getEngine().getInput().isPointerCaptured());
    EXPECT_TRUE(getEngine().getActions().isDown("attack"));
}

// Presses the interface answers itself never reach the actions bound to the same keys and buttons, even while they stay held after the interface lets go, and they reach the actions again once it answers nothing.
TEST_F(ComponentTest, KeepsPressesTheInterfaceAnswersFromActions) {
    getEngine().getActions().load(core::Json::parse(R"({"actions": [{"name": "back", "type": "button", "bindings": ["key:escape", "button:east"]}, {"name": "jump", "type": "button", "bindings": ["key:space", "key:j"]}]})"));
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "combo", "id": "size", "width": 300, "items": [{"id": "small", "text": "Small"}, {"id": "large", "text": "Large"}]},
        {"kind": "textField", "id": "name"},
        {"kind": "button", "id": "play", "text": "Play"}
    ]})");
    const auto reaches = [this](input::Key code, std::string_view action) {
        platform::Event event;
        event.type = platform::Event::Type::KeyDown;
        event.key = code;
        getEngine().handleEvent(event);
        bool reached = false;
        for (int frame = 0; frame < 3; ++frame) {
            frames();
            reached = reached || getEngine().getActions().isDown(action);
        }
        event.type = platform::Event::Type::KeyUp;
        getEngine().handleEvent(event);
        frames();
        return reached;
    };
    const auto reachesButton = [this](input::GamepadButton pressed, std::string_view action) {
        input::GamepadState state{.connected = true};
        state.buttons[static_cast<std::size_t>(pressed)] = true;
        fixture.host().setGamepad(0, state);
        bool reached = false;
        for (int frame = 0; frame < 3; ++frame) {
            frames();
            reached = reached || getEngine().getActions().isDown(action);
        }
        fixture.host().setGamepad(0, input::GamepadState{.connected = true});
        frames();
        return reached;
    };
    // clang-format on

    // The combo keeps the focus once its list closes, so cancel still belongs to the interface until nothing has the focus.
    click(getBounds(*gui, "size").getCenter());
    ASSERT_TRUE(getUi().isCapturingBack());
    EXPECT_FALSE(reaches(input::Key::Escape, "back"));
    EXPECT_FALSE(getUi().isCapturingBack());
    EXPECT_FALSE(reaches(input::Key::Escape, "back"));
    fixture.runLua("require('haylen.ui').clearFocus()");
    frames();
    EXPECT_TRUE(reaches(input::Key::Escape, "back"));

    click(getBounds(*gui, "size").getCenter());
    ASSERT_TRUE(getUi().isCapturingBack());
    EXPECT_FALSE(reachesButton(input::GamepadButton::East, "back"));
    EXPECT_FALSE(getUi().isCapturingBack());
    fixture.runLua("require('haylen.ui').clearFocus()");
    frames();
    EXPECT_TRUE(reachesButton(input::GamepadButton::East, "back"));

    // The keys of a text field being edited stay with it, the escape that ends the editing included.
    click(getBounds(*gui, "name").getCenter());
    EXPECT_FALSE(reaches(input::Key::J, "jump"));
    EXPECT_FALSE(reaches(input::Key::Escape, "back"));

    // Accept presses the focused control, and jumps again once nothing has the focus.
    gui->command(getUi().getContext(), "play", "focus", core::Json::object());
    frames(2);
    events.clear();
    EXPECT_FALSE(reaches(input::Key::Space, "jump"));
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"play:click"}));
    fixture.runLua("require('haylen.ui').clearFocus()");
    frames();
    EXPECT_TRUE(reaches(input::Key::Space, "jump"));

    // A pointer held on empty space edits nothing, so the keys still play the game.
    pointer(platform::Event::Type::MouseMove, {1500.0F, 900.0F});
    pointer(platform::Event::Type::MouseDown, {1500.0F, 900.0F});
    frames(2);
    EXPECT_TRUE(reaches(input::Key::Escape, "back"));
    EXPECT_TRUE(reaches(input::Key::Space, "jump"));
    pointer(platform::Event::Type::MouseUp, {1500.0F, 900.0F});
    frames();
}

TEST_F(ComponentTest, FocusesChipsAndRadioGroups) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "chip", "id": "tag", "text": "Wood"},
        {"kind": "radioGroup", "id": "level", "items": [{"id": "easy", "text": "Easy", "enabled": false}, {"id": "hard", "text": "Hard"}]}
    ]})");
    // clang-format on
    gui->command(getUi().getContext(), "tag", "focus", core::Json::object());
    frames(2);
    key(input::Key::Enter);
    frames();
    EXPECT_EQ(getLastEvent().id, "tag");
    EXPECT_EQ(getLastEvent().value, (core::Json{{"selected", true}}));

    gui->command(getUi().getContext(), "level", "focus", core::Json::object());
    frames(2);
    key(input::Key::Enter);
    frames();
    EXPECT_EQ(getLastEvent().id, "level");
    EXPECT_EQ(getLastEvent().value, (core::Json{{"value", "hard"}}));
}

TEST_F(ComponentTest, ActivatesAScriptedFocusWithTheFirstPress) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "first", "text": "First"},
        {"kind": "button", "id": "second", "text": "Second"}
    ]})");
    const auto ringVisible = [this] {
        return getUi().getFocus().isRingVisible();
    };
    // clang-format on
    const math::Vec2 empty{1500.0F, 900.0F};

    // A mouse player gets a scripted focus without the ring, and the first Enter both shows the ring and presses the button.
    pointer(platform::Event::Type::MouseMove, empty);
    frames();
    gui->command(getUi().getContext(), "first", "focus", core::Json::object());
    frames(2);
    ASSERT_FALSE(ringVisible());
    key(input::Key::Enter);
    EXPECT_TRUE(ringVisible());
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"first:click"}));

    // A click on empty space hides the ring again, and Space works the same way.
    click(empty);
    gui->command(getUi().getContext(), "second", "focus", core::Json::object());
    frames(2);
    ASSERT_FALSE(ringVisible());
    key(input::Key::Space);
    EXPECT_TRUE(ringVisible());
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"first:click", "second:click"}));

    // So does the south button of a gamepad.
    click(empty);
    gui->command(getUi().getContext(), "first", "focus", core::Json::object());
    frames(2);
    ASSERT_FALSE(ringVisible());
    input::GamepadState pressed{.connected = true};
    pressed.buttons[static_cast<std::size_t>(input::GamepadButton::South)] = true;
    fixture.host().setGamepad(0, pressed);
    frames();
    fixture.host().setGamepad(0, input::GamepadState{.connected = true});
    frames();
    EXPECT_TRUE(ringVisible());
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"first:click", "second:click", "first:click"}));
}

TEST_F(ComponentTest, ShowsTheFocusRingOfAScriptedFocusOnlyForGamepadPlayers) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "button", "id": "play", "text": "Play"}]})");
    const math::Rect play = getBounds(*gui, "play");

    // A mouse player gets the focus without the ring, which would otherwise frame the button on start.
    pointer(platform::Event::Type::MouseMove, play.getCenter());
    frames();
    gui->command(getUi().getContext(), "play", "focus", core::Json::object());
    frames(2);
    getUi().getBackend().makeCurrent();
    EXPECT_NE(ImGui::GetCurrentContext()->NavId, 0U);
    EXPECT_FALSE(getUi().getFocus().isRingVisible());

    input::GamepadState start;
    start.connected = true;
    start.buttons[static_cast<std::size_t>(input::GamepadButton::Start)] = true;
    fixture.host().setGamepad(0, start);
    frames();
    fixture.host().setGamepad(0, input::GamepadState{.connected = true});
    frames();
    ASSERT_EQ(getEngine().getInput().getLastDevice(), input::InputDevice::Gamepad);
    EXPECT_FALSE(getUi().getFocus().isRingVisible());

    gui->command(getUi().getContext(), "play", "focus", core::Json::object());
    frames(2);
    EXPECT_TRUE(getUi().getFocus().isRingVisible());
}

TEST_F(ComponentTest, ShowsTheTextAreaPlaceholderWhileEmpty) {
    auto gui = mount(R"({"kind": "textArea", "id": "notes", "rows": 3, "placeholder": "Write here"})");
    const int shown = countDrawn();
    gui->set("notes", {{"placeholder", ""}});
    const int none = countDrawn();
    EXPECT_GT(shown, none);

    // Typed text replaces the placeholder.
    gui->set("notes", {{"placeholder", "Write here"}, {"value", "Day one"}});
    const int typed = countDrawn();
    gui->set("notes", {{"placeholder", ""}});
    EXPECT_EQ(countDrawn(), typed);
}

// Text measures in whole units, so a window smaller than the design, whose glyphs advance by fractions of a unit, draws a label placed at its measured size whole.
TEST_F(ComponentTest, DrawsChoiceLabelsWholeAtTheirMeasuredSize) {
    fixture.host().resize({1280.0F, 720.0F});
    // clang-format off
    const auto count = [this](const std::string& node) {
        auto gui = mount(R"({"kind": "column", "children": [)" + node + "]}");
        const int vertices = countDrawn();
        getUi().unmount(*gui);
        return vertices;
    };
    // clang-format on
    for (const std::string kind : {"toggle", "checkbox"}) {
        for (const std::string text : {"Spin", "flipHorizontal", "Fullscreen", "Music"}) {
            const std::string node = R"({"kind": ")" + kind + R"(", "text": ")" + text + R"(")";
            EXPECT_EQ(count(node + R"(, "align": "start"})"), count(node + R"(, "align": "stretch"})")) << kind << " " << text;
        }
        const std::string cramped = R"({"kind": ")" + kind + R"(", "text": "Fullscreen", "width": 130)";
        EXPECT_LT(count(cramped + "}"), count(cramped + R"(, "width": 900})")) << kind;
    }
    EXPECT_EQ(count(R"({"kind": "statusIndicator", "text": "State: open", "align": "start"})"), count(R"({"kind": "statusIndicator", "text": "State: open", "align": "stretch"})"));
    EXPECT_EQ(count(R"({"kind": "radioGroup", "horizontal": true, "items": [{"id": "a", "text": "Music"}, {"id": "b", "text": "Spin"}]})"), count(R"({"kind": "radioGroup", "items": [{"id": "a", "text": "Music"}, {"id": "b", "text": "Spin"}]})"));
}

TEST_F(ComponentTest, KeepsASliderValueOffItsStepUntilThePlayerMovesIt) {
    auto gui = mount(R"({"kind": "slider", "id": "volume", "width": 400, "value": 0.3499999940395355, "step": 0.05})");
    frames(3);
    EXPECT_TRUE(events.empty());

    // A value the player moves lands on the step.
    const math::Rect volume = getBounds(*gui, "volume");
    click({volume.x + volume.width * 0.8F, volume.getCenter().y});
    ASSERT_EQ(getEventNames(), (std::vector<std::string>{"volume:change"}));
    const double value = getLastEvent().value.at("value").get<double>();
    EXPECT_NEAR(value / 0.05, std::round(value / 0.05), 1e-9);
}

// Left and right land on the step as well, the shown value takes the room of the wider end of the track, and the ends stay finite numbers ImGui can drag between.
TEST_F(ComponentTest, StepsSlidersWithTheKeyboardAndBoundsTheirEnds) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "slider", "id": "volume", "width": 400, "value": 0.33, "step": 0.05},
        {"kind": "slider", "id": "depth", "width": 400, "min": -1000, "max": 0, "value": -1000, "showValue": true, "decimals": 0}
    ]})");
    // clang-format on
    gui->command(getUi().getContext(), "volume", "focus", core::Json::object());
    frames(2);
    key(input::Key::Right);
    key(input::Key::Right);
    ASSERT_FALSE(events.empty());
    const double value = getLastEvent().value.at("value").get<double>();
    EXPECT_NEAR(value / 0.05, std::round(value / 0.05), 1e-9);

    events.clear();
    getUi().getBackend().makeCurrent();
    const float label = Typography::measure(getUi().getContext(), Theme::Font::Body, "-1000").x + getUi().getTheme().getMetric(Theme::Metric::ItemSpacing);
    const math::Rect depth = getBounds(*gui, "depth");
    click({depth.getRight() - label * 0.5F, depth.getCenter().y});
    EXPECT_TRUE(events.empty());

    EXPECT_THROW(gui->set("volume", {{"min", -1e300}}), std::invalid_argument);
    EXPECT_THROW(gui->set("depth", {{"max", 1e16}}), std::invalid_argument);
}

// A growing child of a row measures at the width it draws at, so its wrapped text reports every line and the node below starts after the row, also through a column and a nested row.
TEST_F(ComponentTest, MeasuresGrowingChildrenOfRowsAtTheirShare) {
    const std::string text = "A long line of text that wraps across the narrow middle of the row, several times over, before the row ends and the next node starts.";
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "card", "children": [
        {"kind": "row", "id": "row", "children": [
            {"kind": "button", "text": "Back"},
            {"kind": "column", "grow": 1, "children": [{"kind": "label", "id": "long", "text": ")" + text + R"("}]},
            {"kind": "button", "text": "Wide", "width": 1400}
        ]},
        {"kind": "label", "id": "below", "text": "Below"},
        {"kind": "row", "children": [
            {"kind": "button", "text": "Left", "width": 700},
            {"kind": "row", "grow": 1, "children": [{"kind": "column", "grow": 1, "children": [{"kind": "label", "id": "nested", "text": ")" + text + R"("}]}]}
        ]},
        {"kind": "label", "id": "last", "text": "Last"}
    ]}]})");
    // clang-format on
    frames();
    const float line = Typography::getLineHeight(getUi().getContext(), Theme::Font::Body);
    EXPECT_GE(getBounds(*gui, "long").height, line * 3.0F);
    EXPECT_GE(getBounds(*gui, "below").y, getBounds(*gui, "long").getBottom());
    EXPECT_GE(getBounds(*gui, "nested").height, line * 2.0F);
    EXPECT_GE(getBounds(*gui, "last").y, getBounds(*gui, "nested").getBottom());
}

// A growing child starts from nothing and takes the room its parent leaves, so a growing scroll stays inside a panel that fills the screen, while a container of its own size still fits the content of its growing children.
TEST_F(ComponentTest, GrowsChildrenIntoTheRoomTheirParentLeaves) {
    std::string rows;
    for (int index = 0; index < 60; ++index) {
        rows += std::string(index == 0 ? "" : ", ") + R"({"kind": "label", "text": "Row )" + std::to_string(index) + R"("})";
    }
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "panel", "id": "panel", "grow": 1, "children": [
        {"kind": "label", "text": "Title"},
        {"kind": "scroll", "id": "list", "grow": 1, "children": [{"kind": "column", "children": [)" + rows + R"(]}]}
    ]}]})");
    // clang-format on
    frames();
    EXPECT_EQ(getBounds(*gui, "panel").getBottom(), 1080.0F);
    EXPECT_LE(getBounds(*gui, "list").getBottom(), getBounds(*gui, "panel").getBottom());
    EXPECT_GT(getBounds(*gui, "list").height, 600.0F);

    getUi().unmount(*gui);
    gui = mount(R"({"kind": "column", "children": [{"kind": "card", "id": "card", "children": [{"kind": "label", "id": "first", "grow": 1, "text": "One"}, {"kind": "label", "id": "second", "grow": 3, "text": "Two\nlines"}]}]})");
    frames();
    const float line = Typography::getLineHeight(getUi().getContext(), Theme::Font::Body);
    EXPECT_GE(getBounds(*gui, "first").height, line);
    EXPECT_GE(getBounds(*gui, "second").height, line * 2.0F);
    EXPECT_LE(getBounds(*gui, "second").getBottom(), getBounds(*gui, "card").getBottom());
}

TEST_F(ComponentTest, AcceptsEmptyObjectsAsEmptyLists) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "list", "id": "list", "items": {}},
        {"kind": "tree", "items": [{"id": "root", "text": "Root", "children": {}}], "expanded": {}},
        {"kind": "table", "columns": {}, "rows": [{"id": "r1", "cells": {}}]},
        {"kind": "dialog", "buttons": {}}
    ]})");
    // clang-format on
    EXPECT_EQ(getEngine().getError(), nullptr);
    EXPECT_THROW(gui->set("list", core::Json::parse(R"({"items": {"id": "a"}})")), std::invalid_argument);
    try {
        gui->set("list", core::Json::parse(R"({"items": [{"id": "a"}, {"id": "a"}]})"));
        FAIL() << "A repeated item id was accepted.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The property \"items\" of a \"list\" uses the item id \"a\" more than once.");
    }
}

TEST_F(ComponentTest, LetsThePointerThroughEmptySpace) {
    auto gui = mount(R"({"kind": "stack", "children": [{"kind": "card", "id": "card", "width": 300, "height": 200, "align": "start"}]})");
    pointer(platform::Event::Type::MouseMove, getBounds(*gui, "card").getCenter());
    frames(2);
    EXPECT_TRUE(getUi().isUsingPointer());
    pointer(platform::Event::Type::MouseMove, {1500.0F, 900.0F});
    frames(2);
    EXPECT_FALSE(getUi().isUsingPointer());
    EXPECT_FALSE(getUi().isUsingKeyboard());

    EXPECT_TRUE(getUi().unmount(*gui));
    EXPECT_FALSE(getUi().unmount(*gui));
    EXPECT_FALSE(getUi().isMounted(*gui));
}

class ComponentAssetTest : public ComponentTest {
  protected:
    ComponentAssetTest()
        : ComponentTest({
              {"content/ui/icon.png", toText(test::TestFiles::pngImage(16, 16, 0xFF0000FFU))},
              {"content/ui/panel.png", toText(test::TestFiles::pngImage(24, 24, 0xFFFFFFFFU))},
              {"content/ui/fill.png", toText(test::TestFiles::pngImage(8, 8, 0xFFFFFFFFU))},
              {"content/fonts/ui.ttf", toText(core::EmbeddedFiles::getDefaultFont())},
              {"content/themes/wood.json", R"({"name": "wood", "fontFiles": {"ui": "fonts/ui.ttf"}, "fonts": {"title": {"font": "ui", "size": 64}},
                  "surfaces": {"panel": {"image": "ui/panel.png", "slice": 8}, "button": {"image": "ui/panel.png", "slice": 8, "padding": 4}, "track": {"image": "ui/panel.png", "slice": 4, "padding": 4},
                      "trackFill": {"image": "ui/fill.png", "slice": 2, "colorize": true}, "stickBase": {"image": "ui/panel.png", "slice": 8, "colorize": true}, "tooltip": {"image": "ui/panel.png", "slice": 8, "padding": 6}}})"},
              {"content/themes/broken.json", R"({"name": "broken", "fonts": {"body": {"font": "missing"}}})"},
              {"content/themes/smooth.json", R"({"name": "smooth", "imageFilter": "linear"})"},
              {"content/i18n/en.json", R"({"menu": {"play": "Play {n}"}})"},
          }) {}
};

TEST_F(ComponentAssetTest, LoadsImagesThemesAndTranslations) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "image", "id": "icon", "image": "ui/icon.png", "scale": 2}, {"kind": "icon", "image": "ui/icon.png", "color": "accent"}]})");
    ASSERT_TRUE(fixture.frameUntil([&] { return getBounds(*gui, "icon").width > 0.0F; }));
    EXPECT_EQ(getBounds(*gui, "icon").getSize(), math::Vec2(32.0F, 32.0F));

    EXPECT_EQ(getUi().loadTheme(getEngine(), "themes/wood.json", "light"), "wood");
    getUi().setTheme("wood");
    EXPECT_EQ(getUi().getTheme().getName(), "wood");
    EXPECT_EQ(getUi().getThemes(), (std::vector<std::string>{"dark", "light", "wood"}));
    mount(R"({"kind": "panel", "children": [{"kind": "button", "text": "Wood"}, {"kind": "progress", "value": 0.5, "tone": "danger"}, {"kind": "pageHeader", "title": "Big"}, {"kind": "touchStick", "action": "move"}]})");
    frames(2);
    EXPECT_EQ(getEngine().getError(), nullptr);
    EXPECT_THROW((void)getUi().loadTheme(getEngine(), "themes/broken.json"), std::invalid_argument);
    EXPECT_THROW(getUi().setTheme("marble"), std::invalid_argument);
    EXPECT_THROW((void)getUi().loadTheme(getEngine(), "themes/wood.json", "marble"), std::invalid_argument);

    getEngine().getPlugin<plugins::LocalizationPlugin>().loadFolder(getEngine().getPackage(), "i18n");
    EXPECT_EQ(getUi().getContext().getText({.literal = {}, .key = "menu.play", .arguments = {{"n", 2}}}), "Play 2");
    EXPECT_EQ(getUi().getContext().getText({.literal = "plain"}), "plain");

    mount(R"({"kind": "image", "image": "ui/missing.png"})");
    ASSERT_TRUE(fixture.frameUntil([&] { return getEngine().getError() != nullptr; }));
    EXPECT_NE(std::string_view(getEngine().getError()->what()).find("The UI image \"ui/missing.png\" could not be loaded."), std::string::npos);
}

TEST_F(ComponentAssetTest, LoadsImagesWithTheFilterOfTheTheme) {
    const auto loaded = [&] { return getUi().getContext().getImage("ui/icon.png", {16.0F, 16.0F}); };
    ASSERT_TRUE(fixture.frameUntil([&] { return loaded().isValid(); }));
    EXPECT_EQ(loaded().getOptions().filter, graphics::Texture::Filter::Nearest);

    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/smooth.json"));
    EXPECT_FALSE(loaded().isValid());
    ASSERT_TRUE(fixture.frameUntil([&] { return loaded().isValid(); }));
    EXPECT_EQ(loaded().getOptions().filter, graphics::Texture::Filter::Linear);
}

TEST_F(ComponentAssetTest, PressesImageButtons) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "imageButton", "id": "start", "image": "ui/icon.png", "hoverImage": "ui/panel.png", "text": "Go", "scale": 4}]})");
    ASSERT_TRUE(fixture.frameUntil([&] { return getBounds(*gui, "start").width == 64.0F; }));
    click(*gui, "start");
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"start:click"}));
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(ComponentAssetTest, DrawsCarouselArrowsAbovePagesThatFillIt) {
    mount(R"({"kind": "carousel", "height": 300, "arrows": true, "indicators": false, "children": [{"kind": "image", "image": "ui/icon.png", "fit": "cover"}, {"kind": "label", "text": "Two"}]})");

    // The arrows draw with the font atlas and the page with its image, so the last command of each kind tells which is on top.
    // clang-format off
    const auto last = [this](bool atlas) {
        getUi().getBackend().makeCurrent();
        const ImTextureID font = ImGui::GetIO().Fonts->TexRef.GetTexID();
        const ImVector<ImDrawCmd>& commands = ImGui::FindWindowByName("##haylen-guis")->DrawList->CmdBuffer;
        int found = -1;
        for (int index = 0; index < commands.Size; ++index) {
            found = commands[index].ElemCount > 0 && (commands[index].GetTexID() == font) == atlas ? index : found;
        }
        return found;
    };
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return last(false) >= 0; }));
    EXPECT_GT(last(true), last(false));
}

TEST_F(ComponentAssetTest, DrawsTogglesInTheirTrackInEveryState) {
    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/wood.json"));
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "toggle", "id": "sound", "checked": true}]})");
    const ImTextureID track = getUi().getContext().getTextureReference(getEngine().getAssets().texture("ui/panel.png")).GetTexID();
    const ImTextureID fill = getUi().getContext().getTextureReference(getEngine().getAssets().texture("ui/fill.png")).GetTexID();
    // clang-format off
    const auto drawn = [this](ImTextureID texture) {
        getUi().getBackend().makeCurrent();
        const ImVector<ImDrawCmd>& commands = ImGui::FindWindowByName("##haylen-guis")->DrawList->CmdBuffer;
        return std::any_of(commands.begin(), commands.end(), [texture](const ImDrawCmd& command) { return (command.ElemCount > 0 || command.UserCallback != nullptr) && command.TexRef._TexID == texture; });
    };
    // clang-format on

    // A switch that is on fills the groove of its track, whose frame stays around the fill.
    ASSERT_TRUE(fixture.frameUntil([&] { return drawn(fill); }));
    EXPECT_TRUE(drawn(track));

    // A switch that is off keeps the same track, with an empty groove.
    click(*gui, "sound");
    frames(30);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"sound:change"}));
    EXPECT_TRUE(drawn(track));
    EXPECT_FALSE(drawn(fill));
}

// The track of a switch keeps its place and its size while the switch turns, and the fill of the on state covers the same groove in every frame of the animation instead of growing or shrinking across it.
TEST_F(ComponentAssetTest, KeepsTheTrackOfASwitchStillWhileItTurns) {
    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/wood.json"));
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "toggle", "id": "sound", "checked": true}]})");

    // Every draw of a texture starts with a quad named after it, and the quads after it belong to the same draw.
    // clang-format off
    const auto drawFrame = [this] {
        std::map<std::string, math::Rect, std::less<>> drawn;
        graphics2d::Renderer& renderer = getEngine().getRenderer2D();
        const std::uint64_t overlay = renderer.addCanvasOverlay([&drawn](graphics2d::Renderer& drawing) {
            std::string current;
            drawing.visitDrawn([&](const graphics2d::Renderer::Drawn& quad) {
                current = quad.label.empty() ? current : std::string(quad.label);
                const math::Rect bounds = math::Geometry::bounds(quad.corners);
                const auto found = drawn.find(current);
                drawn.insert_or_assign(current, found == drawn.end() ? bounds : found->second.merged(bounds));
            });
        });
        fixture.frames(1);
        renderer.removeCanvasOverlay(overlay);
        return drawn;
    };
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return drawFrame().contains("ui/fill.png"); }));
    const auto steady = drawFrame();
    ASSERT_TRUE(steady.contains("ui/panel.png"));
    const math::Rect track = steady.at("ui/panel.png");
    const math::Rect groove = steady.at("ui/fill.png");
    EXPECT_EQ(groove, track.inset(math::Insets::uniform(4.0F)));

    click(*gui, "sound");
    int faded = 0;
    for (int frame = 0; frame < 30; ++frame) {
        const auto turning = drawFrame();
        ASSERT_TRUE(turning.contains("ui/panel.png"));
        EXPECT_EQ(turning.at("ui/panel.png"), track) << frame;
        if (turning.contains("ui/fill.png")) {
            EXPECT_EQ(turning.at("ui/fill.png"), groove) << frame;
            ++faded;
        }
    }
    EXPECT_GT(faded, 0);
    EXPECT_FALSE(drawFrame().contains("ui/fill.png"));
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"sound:change"}));
}

TEST_F(ComponentAssetTest, PaintsTooltipsWithTheThemeSurface) {
    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/wood.json"));
    auto gui = mount(R"({"kind": "stack", "children": [{"kind": "label", "id": "hint", "text": "Axe", "tooltip": "Chops trees", "align": "start"}]})");
    pointer(platform::Event::Type::MouseMove, getBounds(*gui, "hint").getCenter());
    frames(45);

    getUi().getBackend().makeCurrent();
    const ImGuiWindow* tooltip = ImGui::FindWindowByName("##Tooltip_00");
    ASSERT_NE(tooltip, nullptr);
    const ImTextureID panel = getUi().getContext().getTextureReference(getEngine().getAssets().texture("ui/panel.png")).GetTexID();
    const ImVector<ImDrawCmd>& commands = tooltip->DrawList->CmdBuffer;
    EXPECT_TRUE(std::any_of(commands.begin(), commands.end(), [panel](const ImDrawCmd& command) { return command.TexRef._TexID == panel; }));
}

} // namespace haylen::ui
