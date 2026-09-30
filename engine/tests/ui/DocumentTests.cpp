#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/ComponentRegistry.hpp"
#include "haylen/ui/Document.hpp"
#include "support/EngineFixture.hpp"
#include "ui/components/BuiltInComponents.hpp"

namespace haylen::ui {

namespace {

class DocumentTest : public ::testing::Test {
  protected:
    DocumentTest() {
        BuiltInComponents::registerAll(registry);
    }

    [[nodiscard]] Document document(const std::string& json) const {
        return Document(registry, core::Json::parse(json));
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

TEST_F(DocumentTest, BuildsTreesWithUniqueIds) {
    // clang-format off
    Document menu = document(R"({"kind": "column", "id": "root", "gap": 8, "children": [
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

    EXPECT_NE(captureError([&] { (void)document(R"({"kind": "column", "children": [{"kind": "label", "id": "a"}, {"kind": "label", "id": "a"}]})"); }).find("used more than once"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)document(R"({"kind": "spaceship"})"); }).find("no UI component kind named \"spaceship\""), std::string::npos);
    EXPECT_NE(captureError([&] { (void)document(R"({"text": "no kind"})"); }).find("needs a kind"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)document(R"([1, 2])"); }).find("must be an object"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)document(R"({"kind": "label", "id": ""})"); }).find("non-empty string"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)document(R"({"kind": "label", "children": [{"kind": "label"}]})"); }).find("at most 0 children"), std::string::npos);
    EXPECT_NE(captureError([&] { (void)document(R"({"kind": "column", "children": {"kind": "label"}})"); }).find("must be a list"), std::string::npos);

    // A document holds 64 levels, the root included.
    // clang-format off
    const auto nested = [](int levels) {
        std::string tree = R"({"kind": "label"})";
        for (int level = 1; level < levels; ++level) {
            tree = R"({"kind": "stack", "children": [)" + tree + "]}";
        }
        return tree;
    };
    // clang-format on
    EXPECT_NO_THROW((void)document(nested(64)));
    EXPECT_NE(captureError([&] { (void)document(nested(65)); }).find("limited to 64 levels"), std::string::npos);
}

TEST_F(DocumentTest, PatchesPropertiesAtomically) {
    Document form = document(R"({"kind": "column", "children": [{"kind": "slider", "id": "volume", "min": 0, "max": 1, "value": 0.5}, {"kind": "touchButton", "id": "attack", "action": "attack"}]})");

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

TEST_F(DocumentTest, ReplacesChildrenAndTheirIds) {
    Document list = document(R"({"kind": "column", "id": "list", "children": [{"kind": "label", "id": "first"}, {"kind": "label", "id": "keep"}]})");
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

// New children start at the level of the node they join and count with every node of the document, so no series of replaces grows it past its limits.
TEST_F(DocumentTest, ReplacesChildrenWithinTheLimitsOfTheDocument) {
    const auto column = [](std::size_t id) { return core::Json::array({core::Json{{"kind", "column"}, {"id", std::to_string(id)}}}); };
    Document chain = document(R"({"kind": "column", "id": "0"})");
    for (std::size_t depth = 1; depth < Document::kMaxDepth; ++depth) {
        chain.replaceChildren(std::to_string(depth - 1), column(depth));
    }
    EXPECT_NE(captureError([&] { chain.replaceChildren(std::to_string(Document::kMaxDepth - 1), column(Document::kMaxDepth)); }).find("limited to 64 levels"), std::string::npos);

    // clang-format off
    const auto labels = [](std::size_t count, bool named) {
        core::Json list = core::Json::array();
        for (std::size_t index = 0; index < count; ++index) {
            list.push_back(named ? core::Json{{"kind", "label"}, {"id", "row" + std::to_string(index)}} : core::Json{{"kind", "label"}});
        }
        return list;
    };
    // clang-format on
    Document rows = document(R"({"kind": "column", "id": "list"})");
    rows.replaceChildren("list", labels(12000, true));
    EXPECT_NO_THROW(rows.replaceChildren("list", labels(12000, true))) << "The replaced rows leave room for the new ones.";

    Document screen = document(R"({"kind": "column", "children": [{"kind": "column", "id": "plain"}, {"kind": "column", "id": "named"}]})");
    screen.replaceChildren("plain", labels(12000, false));
    EXPECT_NE(captureError([&] { screen.replaceChildren("named", labels(8000, false)); }).find("20000 nodes"), std::string::npos) << "Nodes without ids count too.";
}

TEST_F(DocumentTest, ReadsPropertyValuesStrictly) {
    Document sample = document(R"({"kind": "column", "children": [
        {"kind": "label", "id": "count", "text": 42, "width": "auto", "minWidth": 10, "align": "center", "tooltip": {"key": "hint", "args": {"n": 1}}},
        {"kind": "column", "id": "padded", "padding": [4, 8], "gap": 2, "justify": "spaceBetween"},
        {"kind": "divider", "id": "line", "color": "borderStrong"},
        {"kind": "label", "id": "outlined", "outline": "#FF000000", "outlineWidth": 3}
    ]})");
    EXPECT_EQ(sample.find("count")->getAlignment(), Alignment::Center);
    EXPECT_EQ(sample.find("count")->getCommon().tooltip.key, "hint");
    EXPECT_EQ(sample.find("count")->getCommon().tooltip.arguments, (core::Json{{"n", 1}}));
    EXPECT_FALSE(sample.find("count")->getCommon().width.has_value());
    EXPECT_EQ(sample.find("line")->getRowAlignment(), Alignment::Center);

    const auto invalid = [&](const std::string& json) { return captureError([&] { (void)document(json); }); };
    EXPECT_NE(invalid(R"({"kind": "label", "width": -3})").find("The property \"width\" of a \"label\" must be a non-negative number or \"auto\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "align": "middle"})").find("The property \"align\" of a \"label\" must be \"start\", \"center\", \"end\" or \"stretch\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "visible": "yes"})").find("The property \"visible\" of a \"label\" must be \"true\" or \"false\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "color": "purple"})").find("must name a theme color"), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "label", "outline": "dark"})").find("must be a color"), std::string::npos);
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
    EXPECT_NE(invalid(R"({"kind": "dialog", "buttons": [{"id": "ok", "variant": "huge"}]})").find("The property \"buttons\" of a \"dialog\" has a variant other than \"default\", \"primary\", \"destructive\", \"toolbar\", \"icon\" or \"link\"."), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "touchStick"})").find("The property \"action\" of a \"touchStick\" names the virtual stick"), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "numberField", "min": 5, "max": 1})").find("must not be greater than \"max\""), std::string::npos);
    EXPECT_NE(invalid(R"({"kind": "tree", "expanded": [1]})").find("list of item ids"), std::string::npos);
}

TEST(ComponentRegistryTest, CreatesRegisteredKinds) {
    ComponentRegistry registry;
    BuiltInComponents::registerAll(registry);
    const std::vector<std::string> expected{
        "accordion", "alert", "avatar", "badge", "busyIndicator", "button", "card", "carousel", "checkbox", "chip", "circularProgress", "colorField", "column", "combo", "contextMenu", "dialog", "divider", "emptyState", "filterField", "formField", "grid", "icon", "image", "imageButton", "keyCapture", "label", "list", "menuButton", "numberField", "pageHeader", "panel", "popover", "progress", "radioGroup", "rangeSlider", "richText", "row", "safeArea", "scroll", "secretField", "sectionTitle", "segmentedControl", "settingsActions", "settingsForm", "settingsRow", "slider", "slotGrid", "spacer", "splitter", "stack", "statusIndicator", "stepper", "table", "tabs", "textArea", "textField", "toast", "toggle", "touchButton", "touchStick", "tree", "window",
    };
    EXPECT_EQ(registry.getKinds(), expected);
    EXPECT_TRUE(registry.contains("button"));
    EXPECT_EQ(registry.create("button")->getKind(), "button");
    EXPECT_THROW(registry.add("button", [] { return std::unique_ptr<Component>(); }), std::invalid_argument);
    EXPECT_THROW(registry.add("", {}), std::invalid_argument);
}

} // namespace haylen::ui
