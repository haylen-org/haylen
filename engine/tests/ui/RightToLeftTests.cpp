#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Utf8.hpp"
#include "haylen/localization/Catalog.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "support/UiFixture.hpp"
#include "ui/TextFieldLayout.hpp"

namespace haylen::ui {

namespace {

class RightToLeftTest : public ::testing::Test {
  protected:
    [[nodiscard]] std::shared_ptr<text::Font> load(const std::string& name) {
        std::ifstream file(std::string(HAYLEN_TEST_FONTS) + "/" + name, std::ios::binary);
        return std::make_shared<text::TrueTypeFont>(ui.getEngine().getGraphics(), std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));
    }

    [[nodiscard]] TextFieldLayout lay(const std::string& value) {
        return {family->layout(value, {.size = 30.0F}), core::Utf8::decode(value)};
    }

    [[nodiscard]] int getCaret() {
        return ui.getFixture().host().getTextInput().getPublished().back().selectionEnd;
    }

    test::UiFixture ui;
    std::shared_ptr<text::FontFamily> family = std::make_shared<text::FontFamily>(text::FontFamily::Faces{.regular = ui.getEngine().getDefaultFont(), .fallbacks = {load("noto_sans_hebrew_regular.ttf"), load("noto_sans_devanagari_regular.ttf")}});
};

} // namespace

// Right-to-left text starts at the right, so the caret before its first letter stands at the right end and the left arrow moves it forward, while the left-to-right run inside keeps its own order.
TEST_F(RightToLeftTest, PlacesTheCaretOnTheRightClusterOfRightToLeftText) {
    const TextFieldLayout field = lay("שלום abc");
    const text::Layout& laid = field.getLayout();
    ASSERT_TRUE(laid.lines[0].rightToLeft);
    const float right = std::ranges::max(laid.characters, {}, [](const text::Layout::Character& character) { return character.box.getRight(); }).box.getRight();
    const float left = std::ranges::min(laid.characters, {}, [](const text::Layout::Character& character) { return character.box.x; }).box.x;
    EXPECT_NEAR(field.getCaret(0).x, right, 0.5F);
    EXPECT_NEAR(field.getCaret(8).x, left, 0.5F);
    EXPECT_GT(field.getCaret(1).x, field.getCaret(2).x);
    EXPECT_LT(field.getCaret(5).x, field.getCaret(6).x);

    EXPECT_EQ(field.moveAcross(0, false), 1U);
    EXPECT_EQ(field.moveAcross(1, true), 0U);
    EXPECT_EQ(field.moveAcross(8, false), 8U);
    EXPECT_EQ(field.hitTest({left - 20.0F, 10.0F}), 8U);
    EXPECT_EQ(field.hitTest({right + 20.0F, 10.0F}), 0U);
    EXPECT_EQ(field.hitTest({field.getCaret(2).x + 1.0F, 10.0F}), 2U);

    // A selection across the change of direction covers two runs apart on screen.
    EXPECT_EQ(field.getSelection(2, 7).size(), 2U);
    EXPECT_EQ(field.getWord(6), (std::pair<std::size_t, std::size_t>{5, 8}));

    // A Devanagari conjunct with its vowel sign is one cluster, so the caret never stops inside it.
    const TextFieldLayout conjunct = lay("क्षि क");
    EXPECT_EQ(conjunct.moveAcross(0, true), 4U);
    EXPECT_EQ(conjunct.hitTest({conjunct.getCaret(0).x + 2.0F, 10.0F}), 0U);
}

// A text field of a right-to-left node lines its text up on the right, a press left of the text puts the caret at its end, and the arrow keys move the caret on screen.
TEST_F(RightToLeftTest, MovesTheCaretOfARightToLeftFieldOnScreen) {
    auto document = ui.mount(R"({"kind": "column", "direction": "rtl", "padding": 20, "children": [{"kind": "textField", "id": "name", "value": "שלום", "width": 400}]})");
    const math::Rect bounds = ui.getBounds(*document, "name");
    ui.click({bounds.x + 30.0F, bounds.getCenter().y});
    EXPECT_EQ(getCaret(), 4);

    ui.key(input::Key::Right);
    EXPECT_EQ(getCaret(), 3);
    ui.key(input::Key::Right);
    ui.key(input::Key::Right);
    ui.key(input::Key::Right);
    EXPECT_EQ(getCaret(), 0);
    ui.key(input::Key::Left);
    EXPECT_EQ(getCaret(), 1);

    ui.click({bounds.getRight() - 12.0F, bounds.getCenter().y});
    EXPECT_EQ(getCaret(), 0);
}

// A right-to-left UI places rows from the right, lines start and end up on the other sides and grows sliders toward the left, and the automatic direction follows the language.
TEST_F(RightToLeftTest, MirrorsLayoutsInARightToLeftUi) {
    const std::string tree = R"({"kind": "column", "width": 600, "children": [
        {"kind": "row", "id": "row", "gap": 10, "children": [{"kind": "button", "id": "first", "text": "One"}, {"kind": "button", "id": "second", "text": "Two"}]},
        {"kind": "button", "id": "start", "text": "Start", "align": "start"},
        {"kind": "slider", "id": "slider", "width": 300, "value": 0.2}
    ]})";
    auto plain = ui.mount(tree);
    EXPECT_LT(ui.getBounds(*plain, "first").x, ui.getBounds(*plain, "second").x);
    const float startX = ui.getBounds(*plain, "start").x;
    ui.getUi().unmount(*plain);

    ui.getUi().setDirection(text::Direction::RightToLeft);
    auto mirrored = ui.mount(tree);
    EXPECT_GT(ui.getBounds(*mirrored, "first").x, ui.getBounds(*mirrored, "second").x);
    EXPECT_GT(ui.getBounds(*mirrored, "start").x, startX);

    // A press near the left end of a right-to-left slider picks a value near its maximum.
    const math::Rect slider = ui.getBounds(*mirrored, "slider");
    ui.click({slider.x + 12.0F, slider.getCenter().y});
    EXPECT_GT(ui.findLastEvent("change").value.at("value").get<double>(), 0.8);
    ui.getUi().unmount(*mirrored);

    // An automatic direction takes the one the current language declares.
    localization::Catalog& catalog = ui.getEngine().getPlugin<plugins::LocalizationPlugin>().getCatalog();
    catalog.add("en", core::Json::parse(R"({"hello": "Hello"})"));
    catalog.add("ar", core::Json::parse(R"({"@direction": "rtl", "hello": "مرحبا"})"));
    ui.getUi().setDirection(text::Direction::Auto);
    ui.frames();
    EXPECT_FALSE(ui.getUi().getContext().isRightToLeft());
    catalog.setLanguage("ar");
    ui.frames();
    EXPECT_TRUE(ui.getUi().getContext().isRightToLeft());
    EXPECT_EQ(ui.getUi().getContext().getLanguage(), "ar");
    auto localized = ui.mount(tree);
    EXPECT_GT(ui.getBounds(*localized, "first").x, ui.getBounds(*localized, "second").x);

    // A node with a direction of its own keeps it inside a right-to-left UI.
    auto island = ui.mount(R"({"kind": "row", "direction": "ltr", "gap": 10, "children": [{"kind": "button", "id": "first", "text": "One"}, {"kind": "button", "id": "second", "text": "Two"}]})");
    EXPECT_LT(ui.getBounds(*island, "first").x, ui.getBounds(*island, "second").x);
}

} // namespace haylen::ui
