#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/ComponentRegistry.hpp"
#include "haylen/ui/Gui.hpp"
#include "support/EngineFixture.hpp"
#include "support/UiFixture.hpp"
#include "ui/components/BuiltInComponents.hpp"

namespace haylen::ui {

namespace {

class GuiTest : public ::testing::Test {
  protected:
    GuiTest() {
        BuiltInComponents::registerAll(registry);
    }

    [[nodiscard]] Gui gui(const std::string& json) const {
        return Gui(registry, core::Json::parse(json));
    }

    // Runs an action that should fail and returns the message it failed with.
    [[nodiscard]] static std::string captureError(const std::function<void()>& action) {
        try {
            action();
        } catch (const std::exception& error) {
            return error.what();
        }
        return {};
    }

    ComponentRegistry registry;
};

} // namespace

TEST_F(GuiTest, BuildsTreesWithUniqueIds) {
    // clang-format off
    Gui menu = gui(R"({"kind": "column", "id": "root", "gap": 8, "children": [
        {"kind": "label", "id": "title", "text": "Tiny Island", "font": "title"},
        {"kind": "row", "children": [{"kind": "button", "id": "play", "text": "Play", "variant": "primary"}]}
    ]})");
    // clang-format on
    EXPECT_EQ(menu.getRoot().getKind(), "column");
    EXPECT_EQ(menu.find("play")->getKind(), "button");
    EXPECT_EQ(menu.find("missing"), nullptr);
    EXPECT_EQ(*menu.getProperties("title"), (core::Json{{"text", "Tiny Island"}, {"font", "title"}}));
    EXPECT_EQ(menu.getProperties("missing"), nullptr);
    EXPECT_EQ(menu.getPlacement(), Placement::Safe);
    EXPECT_TRUE(menu.isVisible());
    menu.setVisible(false);
    EXPECT_FALSE(menu.isVisible());

    EXPECT_NE(captureError([&] { (void)gui(R"({"kind": "column", "children": [{"kind": "label", "id": "a"}, {"kind": "label", "id": "a"}]})"); }).find("used more than once"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)gui(R"({"kind": "spaceship"})"); }).find("no UI component kind named \"spaceship\""), std::string::npos);
    EXPECT_NE(captureError([&] { (void)gui(R"({"text": "no kind"})"); }).find("needs a kind"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)gui(R"([1, 2])"); }).find("must be an object"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)gui(R"({"kind": "label", "id": ""})"); }).find("non-empty string"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)gui(R"({"kind": "label", "children": [{"kind": "label"}]})"); }).find("at most 0 children"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)gui(R"({"kind": "column", "children": {"kind": "label"}})"); }).find("must be a list"), std::string::npos);

    // A GUI holds 64 levels, the root included.
    // clang-format off
    const auto nested = [](int levels) {
        std::string tree = R"({"kind": "label"})";
        for (int level = 1; level < levels; ++level) {
            tree = R"({"kind": "stack", "children": [)" + tree + "]}";
        }
        return tree;
    };
    // clang-format on
    EXPECT_NO_THROW((void)gui(nested(64)));
    EXPECT_NE(captureError([&] { (void)gui(nested(65)); }).find("limited to 64 levels"), std::string::npos);
}

TEST_F(GuiTest, PatchesPropertiesAtomically) {
    Gui form = gui(R"({"kind": "column", "children": [{"kind": "slider", "id": "volume", "min": 0, "max": 1, "value": 0.5}, {"kind": "touchButton", "id": "attack", "action": "attack"}]})");

    form.set("volume", core::Json{{"value", 0.25}, {"showValue", true}});
    EXPECT_EQ(*form.getProperties("volume"), (core::Json{{"min", 0}, {"max", 1}, {"value", 0.25}, {"showValue", true}}));

    // Checking the merged state lets a patch leave out required properties and still catches values that clash with earlier ones.
    form.set("attack", core::Json{{"size", 90}});
    EXPECT_NE(captureError([&] { form.set("volume", core::Json{{"min", 2}}); }).find("The property \"min\" of a \"slider\" must be smaller than \"max\"."), std::string::npos);
    EXPECT_EQ(form.getProperties("volume")->at("min"), 0);
    EXPECT_NE(captureError([&] { form.set("volume", core::Json{{"value", "loud"}}); }).find("The property \"value\" of a \"slider\" must be a number."), std::string::npos);
    EXPECT_NE(captureError([&] { form.set("volume", core::Json{{"colour", "red"}}); }).find("The property \"colour\" of a \"slider\" does not exist."), std::string::npos);
    EXPECT_NE(captureError([&] { form.set("volume", core::Json{{"kind", "label"}}); }).find("without \"kind\", \"id\" or \"children\""), std::string::npos);
    EXPECT_NE(captureError([&] { form.set("nothing", core::Json::object()); }).find("no node with the id \"nothing\""), std::string::npos);
}

TEST_F(GuiTest, ReplacesChildrenAndTheirIds) {
    Gui list = gui(R"({"kind": "column", "id": "list", "children": [{"kind": "label", "id": "first"}, {"kind": "label", "id": "keep"}]})");
    list.replaceChildren("list", core::Json::parse(R"([{"kind": "label", "id": "first"}, {"kind": "label", "id": "second", "text": "two"}])"));
    EXPECT_NE(list.find("second"), nullptr);
    EXPECT_EQ(list.find("keep"), nullptr);
    EXPECT_EQ(list.getProperties("keep"), nullptr);
    EXPECT_EQ(list.getProperties("second")->at("text"), "two");
    EXPECT_EQ(list.getRoot().getChildren().size(), 2U);

    EXPECT_NE(captureError([&] { list.replaceChildren("list", core::Json::parse(R"([{"kind": "label", "id": "list"}])")); }).find("used more than once"), std::string::npos);
    EXPECT_NE(list.find("second"), nullptr);
    EXPECT_NE(captureError([&] { list.replaceChildren("second", core::Json::parse(R"([{"kind": "label"}])")); }).find("at most 0 children"), std::string::npos);
    EXPECT_NE(captureError([&] { list.replaceChildren("list", core::Json::object()); }).find("list of nodes"), std::string::npos);
}

// New children start at the level of the node they join and count with every node of the GUI, so no series of replaces grows it past its limits.
TEST_F(GuiTest, ReplacesChildrenWithinTheLimitsOfTheGui) {
    const auto column = [](std::size_t id) { return core::Json::array({core::Json{{"kind", "column"}, {"id", std::to_string(id)}}}); };
    Gui chain = gui(R"({"kind": "column", "id": "0"})");
    for (std::size_t depth = 1; depth < Gui::kMaxDepth; ++depth) {
        chain.replaceChildren(std::to_string(depth - 1), column(depth));
    }
    EXPECT_NE(captureError([&] { chain.replaceChildren(std::to_string(Gui::kMaxDepth - 1), column(Gui::kMaxDepth)); }).find("limited to 64 levels"), std::string::npos);

    // clang-format off
    const auto labels = [](std::size_t count, bool named) {
        core::Json list = core::Json::array();
        for (std::size_t index = 0; index < count; ++index) {
            list.push_back(named ? core::Json{{"kind", "label"}, {"id", "row" + std::to_string(index)}} : core::Json{{"kind", "label"}});
        }
        return list;
    };
    // clang-format on
    Gui rows = gui(R"({"kind": "column", "id": "list"})");
    rows.replaceChildren("list", labels(12000, true));
    EXPECT_NO_THROW(rows.replaceChildren("list", labels(12000, true))) << "The replaced rows leave room for the new ones.";

    Gui screen = gui(R"({"kind": "column", "children": [{"kind": "column", "id": "plain"}, {"kind": "column", "id": "named"}]})");
    screen.replaceChildren("plain", labels(12000, false));
    EXPECT_NE(captureError([&] { screen.replaceChildren("named", labels(8000, false)); }).find("20000 nodes"), std::string::npos) << "Nodes without ids count too.";
}

TEST_F(GuiTest, ReadsPropertyValuesStrictly) {
    Gui sample = gui(R"({"kind": "column", "children": [
        {"kind": "label", "id": "count", "text": 42, "width": "auto", "minWidth": 10, "align": "center", "tooltip": {"key": "hint", "args": {"n": 1}}},
        {"kind": "column", "id": "padded", "padding": [4, 8], "gap": 2, "justify": "spaceBetween"},
        {"kind": "divider", "id": "line", "color": "borderStrong"},
        {"kind": "label", "id": "outlined", "outline": "#FF000000", "outlineWidth": 3},
        {"kind": "label", "id": "golden", "gradient": {"colors": ["#FFFFE9A0", "#FFC8861E"]}},
        {"kind": "label", "id": "banded", "gradient": {"stops": [[0, "#FFFFFFFF"], [0.5, "#FF808080"]], "angle": 0}}
    ]})");
    EXPECT_EQ(sample.find("count")->getAlignment(), Alignment::Center);
    EXPECT_EQ(sample.find("count")->getCommon().tooltip.key, "hint");
    EXPECT_EQ(sample.find("count")->getCommon().tooltip.arguments, (core::Json{{"n", 1}}));
    EXPECT_FALSE(sample.find("count")->getCommon().width.has_value());
    EXPECT_EQ(sample.find("line")->getRowAlignment(), Alignment::Center);

    const auto invalid = [&](const std::string& json) { return captureError([&] { (void)gui(json); }); };
    EXPECT_NE(invalid(R"({"kind": "label", "width": -3})").find("The property \"width\" of a \"label\" must be a non-negative number or \"auto\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "align": "middle"})").find("The property \"align\" of a \"label\" must be \"start\", \"center\", \"end\" or \"stretch\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "visible": "yes"})").find("The property \"visible\" of a \"label\" must be \"true\" or \"false\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "color": "purple"})").find("must name a theme color"), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "outline": "dark"})").find("must be a color"), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "gradient": "#FFFFFFFF"})").find("The property \"gradient\" of a \"label\" must be a table with \"colors\" or \"stops\""), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "gradient": {"colors": ["#FFFFFFFF"], "kind": "radial"}})").find("has the unknown key \"kind\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "gradient": {"colors": ["a", "b", "c", "d", "e"]}})").find("holds from 1 to 4 colors."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "gradient": {"stops": [[0.9, "#FFFFFFFF"], [0.1, "#FF000000"]]}})").find("needs offsets from 0 to 1 in increasing order."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "gradient": {"colors": ["#FFFFFFFF"], "angle": "up"}})").find("needs an angle in radians."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "text": ["a"]})").find("The property \"text\" of a \"label\" must be text"), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "text": {"args": {}}})").find("needs a translation key"), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "column", "padding": [1, 2, 3]})").find("one, two or four non-negative numbers"), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "grid", "columns": 1.5})").find("The property \"columns\" of a \"grid\" must be a whole number."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "grid", "columns": 0})").find("The property \"columns\" of a \"grid\" must be from 1 to 64."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "image", "minWidth": -1})").find("The property \"minWidth\" of an \"image\" must be at least 0."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "font": "tiny"})").find("The property \"font\" of a \"label\" must be \"body\", \"caption\", \"button\", \"heading\", \"title\" or \"monospace\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "list", "items": [{"id": "a"}, {"id": "a"}]})").find("The property \"items\" of a \"list\" uses the item id \"a\" more than once."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "list", "items": [{"text": "no id"}]})").find("needs a non-empty id"), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "list", "items": [{"id": "a", "children": []}]})").find("The property \"items\" of a \"list\" cannot nest items, which only a tree does."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "tree", "items": [{"id": "a", "children": 3}]})").find("The property \"items.children\" of a \"tree\" must be a list of items."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "list", "items": [{"id": "a", "icon": "x"}]})").find("Unknown key \"icon\" in \"list.items\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "table", "columns": [{"align": "middle"}]})").find("has an \"align\" other than \"start\""), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "table", "columns": [{"id": "a"}]})").find("Unknown key \"id\" in \"table.columns\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "table", "rows": [{"id": "a", "cells": []}, {"id": "a", "cells": []}]})").find("The property \"rows\" of a \"table\" uses the row id \"a\" more than once."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "table", "columns": [{"text": "Name"}], "rows": [{"id": "a", "cells": ["Ana", 12]}]})").find("The property \"rows\" of a \"table\" gives the row \"a\" 2 cells for 1 columns."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "dialog", "buttons": [{"id": "ok", "variant": "huge"}]})").find("The property \"buttons\" of a \"dialog\" has a variant other than \"default\", \"primary\", \"destructive\", \"toolbar\", \"icon\" or \"link\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "touchStick"})").find("The property \"action\" of a \"touchStick\" names the virtual stick"), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "numberField", "min": 5, "max": 1})").find("must not be greater than \"max\""), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "tree", "expanded": [1]})").find("list of item ids"), std::string::npos);
}

// Children that replace the same children draw the frame they arrive in as the old ones did, laid out and with their pictures and text, so a screen that rebuilds its content never blinks.
TEST(GuiReplaceTest, DrawsReplacedChildrenInTheFrameTheyArrive) {
    test::UiFixture ui;
    // clang-format off
    const std::string rows = R"([
        {"kind": "label", "text": "Your home", "font": "title"},
        {"kind": "card", "children": [{"kind": "row", "children": [{"kind": "label", "text": "Central", "grow": 1}, {"kind": "badge", "text": "Online"}]}]},
        {"kind": "toggle", "text": "Lights", "checked": true},
        {"kind": "progress", "value": 0.4},
        {"kind": "button", "text": "Refresh", "align": "stretch"},
        {"kind": "textField", "value": "Kitchen"}
    ])";
    // clang-format on
    auto gui = ui.mount(R"({"kind": "scroll", "id": "page", "width": 600, "height": 500, "children": [{"kind": "column", "id": "content", "children": )" + rows + "}]}");
    ui.frames(30);

    const ui::Backend& backend = ui.getUi().getBackend();
    // clang-format off
    const auto capture = [&backend] {
        std::vector<std::string> drawn;
        for (const graphics2d::Shape& shape : backend.getShapes()) {
            drawn.push_back("shape " + std::to_string(shape.bounds.x) + " " + std::to_string(shape.bounds.y) + " " + std::to_string(shape.bounds.width) + " " + std::to_string(shape.bounds.height) + " " + shape.color.toHex());
        }
        for (const ui::Backend::Text& text : backend.getTexts()) {
            drawn.push_back("text " + std::string(backend.getContent(text)) + " " + std::to_string(text.origin.x) + " " + std::to_string(text.origin.y));
        }
        return drawn;
    };
    // clang-format on
    const std::vector<std::string> steady = capture();
    gui->replaceChildren("content", core::Json::parse(rows));
    ui.frames(1);
    EXPECT_EQ(capture(), steady);
}

TEST(ComponentRegistryTest, CreatesRegisteredKinds) {
    ComponentRegistry registry;
    BuiltInComponents::registerAll(registry);
    const std::vector<std::string> expected{
        "accordion", "alert", "avatar", "badge", "busyIndicator", "button", "card", "carousel", "checkbox", "chip", "circularProgress", "collection", "colorField", "column", "combo", "contextMenu", "dialog", "divider", "emptyState", "filterField", "formField", "grid", "icon", "image", "imageButton", "keyCapture", "label", "list", "menuButton", "numberField", "pageHeader", "panel", "playArea", "popover", "progress", "radioGroup", "rangeSlider", "richText", "row", "safeArea", "scroll", "secretField", "sectionTitle", "segmentedControl", "settingsActions", "settingsForm", "settingsRow", "slider", "slotGrid", "spacer", "splitter", "stack", "statusIndicator", "stepper", "table", "tabs", "textArea", "textField", "toast", "toggle", "touchButton", "touchStick", "tree", "window",
    };
    EXPECT_EQ(registry.getKinds(), expected);
    EXPECT_TRUE(registry.contains("button"));
    EXPECT_EQ(registry.create("button")->getKind(), "button");
    EXPECT_THROW(registry.add("button", [] { return std::unique_ptr<Component>(); }), std::invalid_argument);
    EXPECT_THROW(registry.add("", {}), std::invalid_argument);
}

TEST(UiPluginTest, RequiresAStartedEngine) {
    plugins::UiPlugin plugin;
    EXPECT_THROW((void)plugin.getBackend(), std::logic_error);
    EXPECT_THROW((void)plugin.getContext(), std::logic_error);
    EXPECT_THROW(plugin.mount(plugin.createGui(core::Json::parse(R"({"kind": "label", "text": "Idle"})"))), std::logic_error);
    EXPECT_THROW(plugin.addFontFamily("empty", nullptr), std::invalid_argument);

    // A theme chosen before the UI starts is the theme it starts with.
    plugin.setTheme("light");
    EXPECT_EQ(plugin.getTheme().getName(), "light");
}

} // namespace haylen::ui
