#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/EmbeddedFiles.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/math/Geometry2.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/platform/Accessibility.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Navigation.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/ComponentFixture.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"
#include "ui/Typography.hpp"

namespace haylen::ui {

class ComponentAssetTest : public ::testing::Test, public test::ComponentFixture {
  protected:
    ComponentAssetTest()
        : ComponentFixture({
              {"content/ui/icon.png", test::TestFiles::toText(test::TestFiles::pngImage(16, 16, 0xFF0000FFU))},
              {"content/ui/panel.png", test::TestFiles::toText(test::TestFiles::pngImage(24, 24, 0xFFFFFFFFU))},
              {"content/ui/fill.png", test::TestFiles::toText(test::TestFiles::pngImage(8, 8, 0xFFFFFFFFU))},
              {"content/fonts/ui.ttf", test::TestFiles::toText(core::EmbeddedFiles::getDefaultFont())},
              {"content/themes/wood.json", R"({"name": "wood", "fontFiles": {"ui": "fonts/ui.ttf"}, "fonts": {"title": {"font": "ui", "size": 64}},
                  "surfaces": {"panel": {"image": "ui/panel.png", "slice": 8}, "button": {"image": "ui/panel.png", "slice": 8, "padding": 4}, "track": {"image": "ui/panel.png", "slice": 4, "padding": 4},
                      "trackFill": {"image": "ui/fill.png", "slice": 2, "colorize": true}, "stickBase": {"image": "ui/panel.png", "slice": 8, "colorize": true}, "tooltip": {"image": "ui/panel.png", "slice": 8, "padding": 6}}})"},
              {"content/ui/mark.png", test::TestFiles::toText(test::TestFiles::pngImage(16, 16, 0xFFFFFFFFU))},
              {"content/ui/dot.png", test::TestFiles::toText(test::TestFiles::pngImage(16, 16, 0xFFFFFFFFU))},
              {"content/ui/row.png", test::TestFiles::toText(test::TestFiles::pngImage(24, 24, 0xFFFFFFFFU))},
              {"content/ui/hover.png", test::TestFiles::toText(test::TestFiles::pngImage(24, 24, 0xFFFFFFFFU))},
              {"content/ui/bar.png", test::TestFiles::toText(test::TestFiles::pngImage(24, 24, 0xFFFFFFFFU))},
              {"content/ui/body.png", test::TestFiles::toText(test::TestFiles::pngImage(24, 24, 0xFFFFFFFFU))},
              {"content/ui/close.png", test::TestFiles::toText(test::TestFiles::pngImage(16, 16, 0xFF0000FFU))},
              {"content/ui/closing.png", test::TestFiles::toText(test::TestFiles::pngImage(16, 16, 0xFF0000FFU))},
              {"content/themes/plates.json", R"({"name": "plates", "surfaces": {"radio": {"image": "ui/mark.png"}, "radioSelected": {"image": "ui/dot.png"}, "cellSelected": {"image": "ui/row.png", "slice": 8},
                  "cellHover": {"image": "ui/hover.png", "slice": 8}, "titleBar": {"image": "ui/bar.png", "slice": 8, "padding": [10, 16, 12, 16]}, "toast": {"image": "ui/panel.png", "slice": 8, "padding": 20},
                  "window": {"image": "ui/body.png", "slice": 8}, "close": {"image": "ui/close.png"}, "closeHover": {"image": "ui/closing.png"}}})"},
              {"content/themes/broken.json", R"({"name": "broken", "fonts": {"body": {"font": "missing"}}})"},
              {"content/themes/smooth.json", R"({"name": "smooth", "imageFilter": "linear"})"},
              {"content/i18n/en.json", R"({"menu": {"play": "Play {n}"}})"},
          }) {}

    // Draws a frame and returns where each texture drawn through the 2D renderer landed, since every draw of a texture starts with a quad named after it and the quads after it belong to the same draw until a text starts another one.
    std::map<std::string, math::Rect, std::less<>> drawFrame() {
        std::map<std::string, math::Rect, std::less<>> drawn;
        graphics2d::Renderer& renderer = getEngine().getRenderer2D();
        // clang-format off
        const std::uint64_t overlay = renderer.addCanvasOverlay([&drawn](graphics2d::Renderer& drawing) {
            std::string current;
            drawing.visitDrawn([&](const graphics2d::Renderer::Drawn& quad) {
                if (quad.text) {
                    current.clear();
                    return;
                }

                current = quad.label.empty() ? current : std::string(quad.label);
                const math::Rect bounds = math::Geometry2::bounds(quad.corners);
                const auto found = drawn.find(current);
                drawn.insert_or_assign(current, found == drawn.end() ? bounds : found->second.merged(bounds));
            });
        });
        // clang-format on
        fixture->frames(1);
        renderer.removeCanvasOverlay(overlay);
        return drawn;
    }
};

TEST_F(ComponentAssetTest, LoadsImagesThemesAndTranslations) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "image", "id": "icon", "image": "ui/icon.png", "scale": 2}, {"kind": "icon", "image": "ui/icon.png", "color": "accent"}]})");
    ASSERT_TRUE(fixture->frameUntil([&] { return getBounds(*gui, "icon").width > 0.0F; }));
    EXPECT_EQ(getBounds(*gui, "icon").getSize(), math::Vec2(32.0F, 32.0F));

    EXPECT_EQ(getUi().loadTheme(getEngine(), "themes/wood.json", "light"), "wood");
    getUi().setTheme("wood");
    EXPECT_EQ(getUi().getTheme().getName(), "wood");
    EXPECT_EQ(getUi().getThemes(), (std::vector<std::string>{"dark", "darkHighContrast", "light", "lightHighContrast", "wood"}));
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
    ASSERT_TRUE(fixture->frameUntil([&] { return getEngine().getError() != nullptr; }));
    EXPECT_NE(std::string_view(getEngine().getError()->what()).find("The UI image \"ui/missing.png\" could not be loaded."), std::string::npos);
}

TEST_F(ComponentAssetTest, LoadsImagesWithTheFilterOfTheTheme) {
    const auto loaded = [&] { return getUi().getContext().getImage("ui/icon.png", {16.0F, 16.0F}); };
    ASSERT_TRUE(fixture->frameUntil([&] { return loaded().isValid(); }));
    EXPECT_EQ(loaded().getOptions().filter, graphics::Texture::Filter::Nearest);

    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/smooth.json"));
    EXPECT_FALSE(loaded().isValid());
    ASSERT_TRUE(fixture->frameUntil([&] { return loaded().isValid(); }));
    EXPECT_EQ(loaded().getOptions().filter, graphics::Texture::Filter::Linear);
}

TEST_F(ComponentAssetTest, PressesImageButtons) {
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "imageButton", "id": "start", "image": "ui/icon.png", "hoverImage": "ui/panel.png", "text": "Go", "scale": 4}]})");
    ASSERT_TRUE(fixture->frameUntil([&] { return getBounds(*gui, "start").width == 64.0F; }));
    click(*gui, "start");
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"start:click"}));
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(ComponentAssetTest, DrawsCarouselArrowsAbovePagesThatFillIt) {
    mount(R"({"kind": "carousel", "height": 300, "arrows": true, "indicators": false, "children": [{"kind": "image", "image": "ui/icon.png", "fit": "cover"}, {"kind": "label", "text": "Two"}]})");

    // The arrows draw as shapes from callbacks of the draw list and the page as a mesh of its image, so the last command of each kind tells which is on top.
    // clang-format off
    const auto last = [this](bool shapes) {
        const ImVector<ImDrawCmd>& commands = test::UiFixture::getGuiDrawList(getUi()).CmdBuffer;
        int found = -1;
        for (int index = 0; index < commands.Size; ++index) {
            found = (shapes ? commands[index].UserCallback != nullptr : commands[index].ElemCount > 0) ? index : found;
        }

        return found;
    };
    // clang-format on
    ASSERT_TRUE(fixture->frameUntil([&] { return last(false) >= 0; }));
    EXPECT_GT(last(true), last(false));
}

// Once a GUI has drawn, its frames allocate nothing more than frames without it: the texts, rich texts, shapes, nine-slices and fades of the frame live in storage that frames reuse, and literal texts reach the draws without copies.
TEST_F(ComponentAssetTest, DrawsFramesWithoutAllocating) {
    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/wood.json"));
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
    auto gui = mount(R"({"kind": "scroll", "height": 1000, "children": [{"kind": "column", "padding": 20, "children": [
        {"kind": "pageHeader", "id": "header", "title": "Settings of the whole app", "caption": "Everything that the app lets you change"},
        {"kind": "label", "text": "A label whose text is longer than a short string", "wrap": false},
        {"kind": "label", "text": "A label that wraps its text over the lines it needs to show it all"},
        {"kind": "row", "children": [{"kind": "button", "text": "Accept everything"}, {"kind": "chip", "text": "Filter chip"}, {"kind": "badge", "text": "12"}, {"kind": "image", "image": "ui/icon.png"}]},
        {"kind": "panel", "children": [{"kind": "slider", "value": 0.4}, {"kind": "rangeSlider", "low": 0.2, "high": 0.6}, {"kind": "progress", "value": 0.3, "text": "Downloading the new levels"}]},
        {"kind": "toggle", "text": "Music and sound effects", "checked": true}, {"kind": "checkbox", "text": "Show the hints of every level", "checked": true},
        {"kind": "textField", "value": "Typed text in the field", "placeholder": "Name"}, {"kind": "secretField", "value": "A secret phrase"},
        {"kind": "numberField", "value": 42}, {"kind": "stepper", "value": 3},
        {"kind": "combo", "items": [{"id": "a", "text": "Alpha channel"}, {"id": "b", "text": "Beta"}], "selected": "a"},
        {"kind": "segmentedControl", "items": [{"id": "a", "text": "Alpha"}, {"id": "b", "text": "Beta"}, {"id": "c", "text": "Gamma"}], "selected": "a"},
        {"kind": "radioGroup", "items": [{"id": "a", "text": "Alpha channel only"}, {"id": "b", "text": "Beta"}], "selected": "a"},
        {"kind": "circularProgress", "value": 0.6, "text": "60%"},
        {"kind": "tabs", "items": [{"id": "a", "text": "General settings"}, {"id": "b", "text": "Audio"}], "selected": "a", "children": [{"kind": "label", "text": "Tab one"}, {"kind": "label", "text": "Tab two"}]},
        {"kind": "list", "items": [{"id": "a", "text": "First row of the list", "image": "ui/icon.png"}, {"id": "b", "text": "Second row of the list", "image": "ui/icon.png", "iconColor": "accent"}]},
        {"kind": "card", "style": {"colors": {"raised": "accentBackground", "text": "accentText"}}, "children": [{"kind": "richText", "text": "Some [b]bold[/b] and [color=#ff0000]red[/color] text inside a card"}]},
        {"kind": "settingsRow", "label": "Volume of the music", "caption": "Applies to every scene", "children": [{"kind": "slider", "value": 0.5}]},
        {"kind": "alert", "title": "Heads up", "message": "Something needs your attention right now"},
        {"kind": "table", "columns": [{"text": "Name of the player"}, {"text": "Score"}], "rows": [{"id": "one", "cells": ["Player number one", "120"]}]},
        {"kind": "tree", "items": [{"id": "root", "text": "Root of the tree", "children": [{"id": "leaf", "text": "Leaf"}]}]},
        {"kind": "emptyState", "title": "Nothing here yet", "message": "Add something to see it here"},
        {"kind": "divider"}, {"kind": "statusIndicator", "text": "Online and ready", "tone": "success"}, {"kind": "avatar", "name": "Ana Lima"},
        {"kind": "label", "text": {"key": "settings.audio.title.of.the.screen"}},
        {"kind": "richText", "text": "Read the [url=https://example.com/terms]terms of the service[/url] before you start"},
        {"kind": "stepper", "items": [{"id": "a", "text": "The easiest difficulty"}, {"id": "b", "text": "The hardest difficulty"}], "selected": "a"},
        {"kind": "formField", "label": "The name that the board shows", "required": true, "children": [{"kind": "textField", "placeholder": "Name"}]},
        {"kind": "keyCapture", "value": "key:leftShift"}, {"kind": "numberField", "value": 123456789012345678901234567890.5, "decimals": 2},
        {"kind": "window", "title": "A window with a long title", "x": 100, "y": 100, "width": 500, "height": 300}
    ]}]})");
    // clang-format on
    frames(30);
    EXPECT_EQ(getEngine().getError(), nullptr);
    EXPECT_GT(getBounds(*gui, "header").height, 0.0F);
    EXPECT_FALSE(getUi().getBackend().getFades().empty());
    EXPECT_EQ(measure(), empty);

    // The elements that screen readers read build every frame without allocating once they settled.
    fixture->host().setAccessibilityTreeWanted(true);
    EXPECT_EQ(measure(), empty);
    EXPECT_GT(getEngine().getAccessibility().getElements().size(), 30U);
}

TEST_F(ComponentAssetTest, DrawsTogglesInTheirTrackInEveryState) {
    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/wood.json"));
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "toggle", "id": "sound", "checked": true}]})");
    const ImTextureID track = getUi().getContext().getTextureReference(getEngine().getAssets().texture("ui/panel.png")).GetTexID();
    const ImTextureID fill = getUi().getContext().getTextureReference(getEngine().getAssets().texture("ui/fill.png")).GetTexID();
    // clang-format off
    const auto drawn = [this](ImTextureID texture) {
        const ImVector<ImDrawCmd>& commands = test::UiFixture::getGuiDrawList(getUi()).CmdBuffer;
        return std::any_of(commands.begin(), commands.end(), [texture](const ImDrawCmd& command) { return (command.ElemCount > 0 || command.UserCallback != nullptr) && command.TexRef._TexID == texture; });
    };
    // clang-format on

    // A switch that is on fills the groove of its track, whose frame stays around the fill.
    ASSERT_TRUE(fixture->frameUntil([&] { return drawn(fill); }));
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
    ASSERT_TRUE(fixture->frameUntil([&] { return drawFrame().contains("ui/fill.png"); }));
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

TEST_F(ComponentAssetTest, DrawsRadioMarksWithTheirSurfaces) {
    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/plates.json"));
    mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "radioGroup", "items": [{"id": "easy", "text": "Easy"}, {"id": "hard", "text": "Hard"}], "selected": "easy"}]})");
    ASSERT_TRUE(fixture->frameUntil([&] { return drawFrame().contains("ui/dot.png"); }));
    const auto drawn = drawFrame();
    const float size = getUi().getTheme().getMetric(Theme::Metric::ChoiceSize);
    ASSERT_TRUE(drawn.contains("ui/mark.png"));
    EXPECT_EQ(drawn.at("ui/dot.png").getSize(), math::Vec2(size, size));
    EXPECT_EQ(drawn.at("ui/mark.png").getSize(), math::Vec2(size, size));
    EXPECT_GT(drawn.at("ui/mark.png").y, drawn.at("ui/dot.png").getBottom());
}

TEST_F(ComponentAssetTest, HighlightsTheRowsOfListsWithTheCellSurfaces) {
    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/plates.json"));
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [{"kind": "list", "id": "crew", "width": 400, "height": 300, "items": [{"id": "ana", "text": "Ana"}, {"id": "leo", "text": "Leo"}], "selected": "ana"}]})");
    ASSERT_TRUE(fixture->frameUntil([&] { return drawFrame().contains("ui/row.png"); }));
    const math::Rect chosen = drawFrame().at("ui/row.png");
    EXPECT_FLOAT_EQ(chosen.height, getUi().getTheme().getMetric(Theme::Metric::ListRowHeight));
    EXPECT_FLOAT_EQ(chosen.y, getBounds(*gui, "crew").y);

    pointer(platform::Event::Type::MouseMove, {chosen.getCenter().x, chosen.getCenter().y + chosen.height});
    ASSERT_TRUE(fixture->frameUntil([&] { return drawFrame().contains("ui/hover.png"); }));
    EXPECT_EQ(drawFrame().at("ui/hover.png"), chosen.translated({0.0F, chosen.height}));
}

TEST_F(ComponentAssetTest, FramesTheTitlesOfWindowsAndDialogsWithTheTitleBar) {
    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/plates.json"));
    auto gui = mount(R"({"kind": "window", "id": "bag", "title": "Bag", "x": 100, "y": 100, "width": 500, "height": 400})");
    ASSERT_TRUE(fixture->frameUntil([&] { return drawFrame().contains("ui/bar.png"); }));
    const math::Rect window = getBounds(*gui, "bag");
    EXPECT_EQ(drawFrame().at("ui/bar.png"), math::Rect(window.x, window.y, window.width, getUi().getTheme().getMetric(Theme::Metric::WindowTitleHeight)));

    getUi().unmount(*gui);
    gui = mount(R"({"kind": "dialog", "id": "quit", "title": "Quit?", "message": "Progress is saved.", "buttons": [{"id": "no", "text": "Stay"}]})");
    frames(30);
    const math::Rect dialog = getBounds(*gui, "quit");
    const float padding = getUi().getTheme().getMetric(Theme::Metric::PanelPadding);
    const float title = Typography::measureParagraph(getUi().getContext(), Theme::Font::Heading, "Quit?", dialog.width).y;

    // The body of a card in the middle of the screen starts at the whole unit that ImGui places its child window at.
    const math::Rect bar = drawFrame().at("ui/bar.png");
    EXPECT_EQ(bar.x, dialog.x + padding);
    EXPECT_NEAR(bar.y, dialog.y + padding, 0.5F);
    EXPECT_EQ(bar.width, dialog.width - padding * 2.0F);
    EXPECT_EQ(bar.height, title + 22.0F);
}

TEST_F(ComponentAssetTest, CapsWindowsWithTheTitleBarAndTheirCloseButtonOfTheTheme) {
    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/plates.json"));
    auto gui = mount(R"({"kind": "window", "id": "bag", "title": "Bag", "x": 100, "y": 100, "width": 500, "height": 400, "closable": true})");
    ASSERT_TRUE(fixture->frameUntil([&] { return drawFrame().contains("ui/close.png"); }));
    const math::Rect window = getBounds(*gui, "bag");
    const float header = getUi().getTheme().getMetric(Theme::Metric::WindowTitleHeight);
    const float side = header - 10.0F - 12.0F;
    const math::Rect button = math::Rect(window.getRight() - 16.0F - side, window.y + 10.0F, side, side).expanded(-side * 0.15F);
    const auto covers = [&button](const math::Rect& drawn) { return (drawn.getMin() - button.getMin()).getLength() + (drawn.getMax() - button.getMax()).getLength() < 0.01F; };
    EXPECT_EQ(drawFrame().at("ui/body.png"), math::Rect(window.x, window.y + header * 0.5F, window.width, window.height - header * 0.5F));
    EXPECT_TRUE(covers(drawFrame().at("ui/close.png")));

    pointer(platform::Event::Type::MouseMove, button.getCenter());
    ASSERT_TRUE(fixture->frameUntil([&] { return drawFrame().contains("ui/closing.png"); }));
    EXPECT_TRUE(covers(drawFrame().at("ui/closing.png")));
    EXPECT_FALSE(drawFrame().contains("ui/close.png"));
    click(button.getCenter());
    EXPECT_EQ(getEventNames(), (std::vector<std::string>{"bag:close"}));
}

// A theme that paints messages with an image keeps their text inside the padding of the image, which screen readers find as the frame of the alert.
TEST_F(ComponentAssetTest, KeepsTheTextOfMessagesInsideThePaddingOfTheirSurface) {
    fixture->host().setAccessibilityTreeWanted(true);
    static_cast<void>(getUi().getNavigation().showMessage({{"text", "Saved"}, {"duration", 0}}));
    // clang-format off
    const auto measure = [this] {
        const auto& trees = fixture->host().getAccessibilityTrees();
        if (trees.empty()) {
            throw std::runtime_error("The platform received no accessibility tree.");
        }

        const std::vector<platform::AccessibilityElement>& elements = trees.back().elements;
        const auto found = std::ranges::find(elements, platform::AccessibilityElement::Role::Alert, &platform::AccessibilityElement::role);
        return found == elements.end() ? 0.0F : found->bounds.height;
    };
    // clang-format on
    frames(30);
    const float flat = measure();
    EXPECT_GT(flat, 0.0F);

    getUi().setTheme(getUi().loadTheme(getEngine(), "themes/plates.json"));
    frames(2);
    EXPECT_FLOAT_EQ(measure(), flat + 40.0F);
}

} // namespace haylen::ui
