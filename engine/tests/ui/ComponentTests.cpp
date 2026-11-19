#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/ui/Backend.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/ComponentFixture.hpp"
#include "support/EngineFixture.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

class ComponentTest : public ::testing::Test, public test::ComponentFixture {};

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
        {"kind": "dialog", "title": "Quit?", "message": "Progress is saved.", "buttons": [{"id": "no", "text": "Stay"}, {"id": "yes", "text": "Quit", "variant": "destructive"}], "children": [{"kind": "label", "text": "Really"}]}
    ]}]})");
    // clang-format on
    frames(3);
    EXPECT_EQ(getEngine().getError(), nullptr) << getEngine().getError()->what();
    EXPECT_GT(getEngine().getRenderer2D().getStats().instances, 0U);
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

TEST_F(ComponentTest, RunsDialogsAndPopups) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "dialog", "id": "quit", "title": "Quit?", "buttons": [{"id": "no", "text": "Stay"}, {"id": "yes", "text": "Quit"}]},
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

// A menu whose node stops drawing closes, so it no longer keeps the pointer and cancel from the rest of the UI.
TEST_F(ComponentTest, ClosesThePopupsOfNodesThatStopDrawing) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "play", "text": "Play"},
        {"kind": "menuButton", "id": "menu", "text": "Menu", "items": [{"id": "save", "text": "Save"}]}
    ]})");
    // clang-format on
    frames(2);
    click(*gui, "menu");
    ASSERT_TRUE(getUi().isCapturingBack());
    gui->set("menu", {{"visible", false}});
    frames(2);
    EXPECT_FALSE(getUi().isCapturingBack());
    click(*gui, "play");
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"play:click"}));

    gui->set("menu", {{"visible", true}});
    frames(2);
    click(*gui, "menu");
    ASSERT_TRUE(getUi().isCapturingBack());
    gui->setVisible(false);
    frames(2);
    EXPECT_FALSE(getUi().isCapturingBack());

    gui->setVisible(true);
    frames(2);
    click(*gui, "menu");
    ASSERT_TRUE(getUi().isCapturingBack());
    getUi().unmount(*gui);
    frames(2);
    EXPECT_FALSE(getUi().isCapturingBack());
}

TEST_F(ComponentTest, ScrollsTheContentOfDialogsTallerThanTheScreen) {
    std::string buttons;
    for (int index = 0; index < 30; ++index) {
        buttons += std::string(index == 0 ? "" : ", ") + R"({"kind": "button", "id": "b)" + std::to_string(index) + R"(", "text": "Rule"})";
    }

    auto gui = mount(R"({"kind": "column", "children": [{"kind": "dialog", "id": "rules", "title": "Rules", "buttons": [{"id": "ok", "text": "OK"}], "children": [)" + buttons + "]}]}");
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

TEST_F(ComponentTest, ScrollsAScrollToAnOffsetOrANodeOnCommand) {
    core::Json lines = core::Json::array();
    for (int index = 1; index <= 100; ++index) {
        lines.push_back({{"kind", "label"}, {"id", "line-" + std::to_string(index)}, {"text", "Line " + std::to_string(index)}});
    }

    const core::Json tree = {{"kind", "scroll"}, {"id", "log"}, {"scrollbar", false}, {"children", {{{"kind", "column"}, {"gap", 0}, {"children", lines}}}}};
    auto gui = mount(tree.dump());
    Context& context = getUi().getContext();
    const auto distance = [&](const std::string& id) { return getBounds(*gui, id).y - getBounds(*gui, "log").y; };

    const float offset = distance("line-11");
    gui->command(context, "log", "scrollTo", {{"offset", offset}, {"animated", false}});
    frames(2);
    EXPECT_NEAR(distance("line-11"), 0.0F, 1.0F);

    // A smooth scroll to a node aligns it with the view over a few frames.
    gui->command(context, "log", "scrollTo", {{"node", "line-30"}, {"align", "start"}});
    frames();
    EXPECT_GT(distance("line-30"), 1.0F);
    frames(120);
    EXPECT_NEAR(distance("line-30"), 0.0F, 1.0F);

    // A node that shows whole stays where it is with the nearest alignment.
    gui->command(context, "log", "scrollTo", {{"node", "line-31"}});
    frames(2);
    EXPECT_NEAR(distance("line-30"), 0.0F, 1.0F);
    gui->command(context, "log", "scrollTo", {{"node", "line-20"}, {"align", "center"}, {"animated", false}});
    frames(2);
    EXPECT_NEAR(distance("line-20"), (getBounds(*gui, "log").height - getBounds(*gui, "line-20").height) * 0.5F, 1.0F);
    gui->command(context, "log", "scrollTo", {{"node", "line-1"}, {"animated", false}});
    frames(2);
    EXPECT_NEAR(distance("line-1"), 0.0F, 1.0F);

    EXPECT_THROW(gui->command(context, "log", "scrollTo", core::Json::object()), std::invalid_argument);
    EXPECT_THROW(gui->command(context, "log", "scrollTo", {{"node", "line-1"}, {"offset", 0}}), std::invalid_argument);
    EXPECT_THROW(gui->command(context, "log", "scrollTo", {{"node", "line-999"}}), std::invalid_argument);
    EXPECT_THROW(gui->command(context, "log", "scrollTo", {{"offset", 10}, {"speed", 2}}), std::invalid_argument);
    EXPECT_THROW(gui->command(context, "log", "scrollTo", {{"node", "line-2"}, {"align", "middle"}}), std::invalid_argument);
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(ComponentTest, RefusesTableRowsWithMoreCellsThanColumns) {
    auto gui = mount(R"({"kind": "table", "id": "scores", "columns": [{"text": "Name"}, {"text": "Score"}], "rows": [{"id": "ana", "cells": ["Ana", 12]}, {"id": "bia", "cells": ["Bia"]}]})");
    EXPECT_THROW(gui->set("scores", {{"columns", {{{"text", "Name"}}}}}), std::invalid_argument);
    EXPECT_THROW(gui->set("scores", {{"rows", {{{"id", "leo"}, {"cells", {"Leo", 3, "extra"}}}}}}), std::invalid_argument);

    // A refused change keeps the columns and the rows the table had.
    gui->set("scores", {{"columns", {{{"text", "Name"}}, {{"text", "Score"}}, {{"text", "Rank"}}}}, {"rows", {{{"id", "leo"}, {"cells", {"Leo", 3, 1}}}}}});
    frames();
    EXPECT_EQ(getEngine().getError(), nullptr);
}

// Typing reports only numbers inside the range, and the end of the editing brings any other number into it. Text that is not a finite number changes nothing. The value shows centered, so a click on the field lands inside it and End moves the caret past it.
TEST_F(ComponentTest, CommitsTypedNumbersInsideTheRange) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "numberField", "id": "count", "value": 20, "min": 10, "max": 100}]})");
    // clang-format off
    const auto retype = [this, &gui](std::u32string_view text) {
        click(*gui, "count");
        key(input::Key::End);
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

// A number field shows every digit of a huge value, so its text reads back as the same number.
TEST_F(ComponentTest, ShowsEveryDigitOfAHugeNumber) {
    mount(R"({"kind": "column", "children": [{"kind": "numberField", "id": "big", "value": 1e80, "max": 1e81, "width": 900}]})");
    frames();
    const Backend& backend = getUi().getBackend();
    const auto found = std::ranges::find_if(backend.getTexts(), [&backend](const Backend::Text& text) { return backend.getContent(text).starts_with("1000"); });
    ASSERT_NE(found, backend.getTexts().end());
    EXPECT_EQ(std::stod(std::string(backend.getContent(*found))), 1e80);
}

// A text area wraps a line longer than its width between its words, and every line draws inside the field.
TEST_F(ComponentTest, WrapsTheLinesOfATextArea) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "textArea", "id": "notes", "width": 400, "rows": 4, "value": "The tide rose over the harbor wall and the boats drifted toward the lighthouse at dusk."}]})");
    frames();
    const math::Rect field = getBounds(*gui, "notes");
    Backend& backend = getUi().getBackend();
    bool drawn = false;
    for (const Backend::Text& text : backend.getTexts()) {
        const std::string_view content = backend.getContent(text);
        if (!content.starts_with("The tide")) {
            continue;
        }

        drawn = true;
        const std::shared_ptr<const text::Layout> laid = text.family->layout(content, text.style);
        EXPECT_GT(laid->lines.size(), 2U);

        for (const text::Layout::Line& line : laid->lines) {
            const math::Rect box = line.box.translated(text.origin);
            EXPECT_GE(box.x, field.x);
            EXPECT_LE(box.getRight(), field.getRight());
        }
    }

    EXPECT_TRUE(drawn);
}

// The actions of a form center on one line, so a link beside taller buttons stands on their middle line.
TEST_F(ComponentTest, CentersTheActionsOfASettingsFormOnOneLine) {
    // clang-format off
    auto gui = mount(R"({"kind": "settingsForm", "children": [
        {"kind": "settingsRow", "label": "Music", "children": [{"kind": "toggle"}]},
        {"kind": "settingsActions", "children": [{"kind": "button", "id": "defaults", "text": "Defaults", "variant": "link"}, {"kind": "button", "id": "cancel", "text": "Cancel"}, {"kind": "button", "id": "save", "text": "Save", "variant": "primary"}]}
    ]})");
    // clang-format on
    frames();
    const float middle = getBounds(*gui, "save").getCenter().y;
    EXPECT_NEAR(getBounds(*gui, "defaults").getCenter().y, middle, 1.0F);
    EXPECT_NEAR(getBounds(*gui, "cancel").getCenter().y, middle, 1.0F);
    EXPECT_LT(getBounds(*gui, "defaults").height, getBounds(*gui, "save").height);
}

// Actions that do not fit in one row stack across the width of the form, with the main action on top.
TEST_F(ComponentTest, StacksTheActionsOfANarrowSettingsForm) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "settingsForm", "id": "form", "width": 240, "children": [
        {"kind": "settingsActions", "children": [{"kind": "button", "id": "defaults", "text": "Restore the defaults", "variant": "link"}, {"kind": "button", "id": "cancel", "text": "Cancel"}, {"kind": "button", "id": "save", "text": "Save the settings", "variant": "primary"}]}
    ]}]})");
    // clang-format on
    frames();
    const math::Rect form = getBounds(*gui, "form");
    const math::Rect save = getBounds(*gui, "save");
    const math::Rect cancel = getBounds(*gui, "cancel");
    const math::Rect defaults = getBounds(*gui, "defaults");
    EXPECT_LT(save.getBottom(), cancel.y);
    EXPECT_LT(cancel.getBottom(), defaults.y);

    for (const math::Rect& action : {save, cancel, defaults}) {
        EXPECT_EQ(action.x, save.x);
        EXPECT_EQ(action.width, save.width);
        EXPECT_GE(action.x, form.x);
        EXPECT_LE(action.getRight(), form.getRight());
    }
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

// A theme without pictures of the stick draws its base as a ring around a guide ring, and its knob raised with a rim in the accent color.
TEST_F(ComponentTest, DrawsAFlatStickWithARaisedKnob) {
    mount(R"({"kind": "column", "children": [{"kind": "touchStick", "id": "stick", "action": "move", "radius": 100}]})");
    frames();
    const Theme& theme = getUi().getTheme();
    int rims = 0;
    int rings = 0;
    for (const graphics2d::Shape& shape : getUi().getBackend().getShapes()) {
        rims += shape.borderColor == theme.getColor(Theme::Color::Accent) && shape.bounds.width == 90.0F ? 1 : 0;
        rings += shape.borderWidth > 0.0F && shape.bounds.width == 110.0F ? 1 : 0;
    }

    EXPECT_EQ(rims, 1);
    EXPECT_EQ(rings, 1);
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

// Held touch controls set the same virtual input every frame without allocating, and let go of it once released.
TEST_F(ComponentTest, HoldsTouchControlsWithoutAllocating) {
    // clang-format off
    const auto measure = [this] {
        frames(5);
        const test::AllocationTracker tracker;
        frames(10);
        return tracker.getTotal();
    };
    // clang-format on
    const std::size_t empty = measure();

    // clang-format off
    auto gui = mount(R"({"kind": "row", "padding": 40, "gap": 400, "children": [
        {"kind": "touchStick", "id": "stick", "action": "moveTheCharacterAround", "radius": 100, "deadZone": 0},
        {"kind": "touchButton", "id": "attack", "action": "attackTheNearestEnemy", "size": 120}
    ]})");
    // clang-format on
    const math::Rect stick = getBounds(*gui, "stick");
    const math::Rect attack = getBounds(*gui, "attack");
    touch(platform::Event::Type::TouchBegan, 1, stick.getCenter() + math::Vec2{50.0F, 0.0F});
    touch(platform::Event::Type::TouchBegan, 2, attack.getCenter());
    EXPECT_EQ(measure(), empty);

    input::VirtualInput& controls = getEngine().getVirtualInput();
    EXPECT_TRUE(controls.isButtonDown("attackTheNearestEnemy"));
    EXPECT_GT(controls.getStick("moveTheCharacterAround").x, 0.0F);
    touch(platform::Event::Type::TouchEnded, 1, stick.getCenter() + math::Vec2{50.0F, 0.0F});
    touch(platform::Event::Type::TouchEnded, 2, attack.getCenter());
    frames(2);
    EXPECT_FALSE(controls.isButtonDown("attackTheNearestEnemy"));
    EXPECT_EQ(controls.getStick("moveTheCharacterAround"), math::Vec2());
}

TEST_F(ComponentTest, SlideInButtonsTakeAFingerThatMovesOntoThem) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "padding": 40, "gap": 40, "children": [
        {"kind": "touchButton", "id": "jump", "action": "jump", "size": 120, "slideIn": true},
        {"kind": "touchButton", "id": "attack", "action": "attack", "size": 120, "slideIn": true},
        {"kind": "touchButton", "id": "defend", "action": "defend", "size": 120}
    ]})");
    // clang-format on
    const math::Rect jump = getBounds(*gui, "jump");
    const math::Rect attack = getBounds(*gui, "attack");
    const math::Rect defend = getBounds(*gui, "defend");
    input::VirtualInput& controls = getEngine().getVirtualInput();

    // A thumb that lands on one button and slides onto the next presses the second and lets go of the first.
    touch(platform::Event::Type::TouchBegan, 1, jump.getCenter());
    frames(2);
    EXPECT_TRUE(controls.isButtonDown("jump"));
    touch(platform::Event::Type::TouchMoved, 1, attack.getCenter());
    frames(2);
    EXPECT_FALSE(controls.isButtonDown("jump"));
    EXPECT_TRUE(controls.isButtonDown("attack"));

    // A button without sliding fingers waits for a finger that lands on it.
    touch(platform::Event::Type::TouchMoved, 1, defend.getCenter());
    frames(2);
    EXPECT_FALSE(controls.isButtonDown("attack"));
    EXPECT_FALSE(controls.isButtonDown("defend"));
    touch(platform::Event::Type::TouchEnded, 1, defend.getCenter());
    frames(2);
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"jump:press", "jump:release", "attack:press", "attack:release"}));
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

TEST_F(ComponentTest, TouchControlsLetGoWhenTheUiStops) {
    // clang-format off
    auto gui = mount(R"({"kind": "row", "padding": 40, "gap": 400, "children": [
        {"kind": "touchStick", "id": "stick", "action": "move", "radius": 100, "deadZone": 0},
        {"kind": "touchButton", "id": "attack", "action": "attack", "size": 120}
    ]})");
    // clang-format on
    input::VirtualInput& controls = getEngine().getVirtualInput();
    touch(platform::Event::Type::TouchBegan, 1, getBounds(*gui, "stick").getCenter() + math::Vec2{50.0F, 0.0F});
    touch(platform::Event::Type::TouchBegan, 2, getBounds(*gui, "attack").getCenter());
    frames(2);
    ASSERT_TRUE(controls.isButtonDown("attack"));

    getUi().stop(getEngine());
    EXPECT_FALSE(controls.isButtonDown("attack"));
    EXPECT_EQ(controls.getStick("move"), math::Vec2());
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

TEST_F(ComponentTest, KeepsAMousePressWithTheSideThatTookItUntilItIsReleased) {
    getEngine().getActions().load(core::Json::parse(R"({"actions": [{"name": "attack", "type": "button", "bindings": ["mouse:left"]}]})"));
    auto gui = mount(R"({"kind": "stack", "children": [{"kind": "card", "id": "card", "width": 300, "height": 200, "align": "start"}]})");
    const math::Vec2 card = getBounds(*gui, "card").getCenter();

    // A drag that starts in the game keeps its action down while it crosses the interface.
    pointer(platform::Event::Type::MouseMove, {1500.0F, 900.0F});
    frames(2);
    pointer(platform::Event::Type::MouseDown, {1500.0F, 900.0F});
    frames();
    ASSERT_TRUE(getEngine().getActions().isDown("attack"));
    pointer(platform::Event::Type::MouseMove, card);
    frames(2);
    EXPECT_TRUE(getEngine().getInput().isPointerCaptured());
    EXPECT_TRUE(getEngine().getActions().isDown("attack"));
    EXPECT_FALSE(getEngine().getActions().isPressed("attack"));
    pointer(platform::Event::Type::MouseUp, card);
    frames(2);

    // A press on the interface stays with it when the pointer leaves.
    pointer(platform::Event::Type::MouseDown, card);
    frames();
    pointer(platform::Event::Type::MouseMove, {1500.0F, 900.0F});
    frames(2);
    EXPECT_FALSE(getEngine().getInput().isPointerCaptured());
    EXPECT_FALSE(getEngine().getActions().isDown("attack"));
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

        // A label wider than a cramped choice wraps onto more lines and still draws every letter.
        const std::string cramped = R"({"kind": ")" + kind + R"(", "text": "Fullscreen", "width": 130)";
        EXPECT_EQ(count(cramped + "}"), count(cramped + R"(, "width": 900})")) << kind;
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

// A slider without a step keeps the digits that its decimals show, however small its range.
TEST_F(ComponentTest, DragsASliderOverASmallRangeToEveryDigit) {
    auto gui = mount(R"({"kind": "slider", "id": "volume", "width": 400, "min": 0, "max": 0.001, "decimals": 6})");
    const math::Rect volume = getBounds(*gui, "volume");
    click({volume.x + volume.width * 0.37F, volume.getCenter().y});
    ASSERT_EQ(getEventNames(), (std::vector<std::string>{"volume:change"}));
    const double value = getLastEvent().value.at("value").get<double>();
    EXPECT_GT(value, 0.0);
    EXPECT_LT(value, 0.001);
    EXPECT_GT(std::fabs(value * 1e4 - std::round(value * 1e4)), 1e-6);
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

} // namespace haylen::ui
