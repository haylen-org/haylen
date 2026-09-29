#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include <imgui.h>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::ui {

namespace {

// Hands every theme image the same texture and remembers what was asked for.
struct Loader {
    graphics::Texture texture;
    std::vector<std::string> paths;
    std::vector<graphics::Texture::Options> options;

    Theme::TextureLoader function() {
        // clang-format off
        return [this](std::string_view path, graphics::Texture::Options requested) {
            paths.emplace_back(path);
            options.push_back(requested);
            return texture;
        };
        // clang-format on
    }
};

} // namespace

// The last name of every family reads as its last role, so no role is left without a name.
TEST(ThemeTest, ReadsEveryRoleByName) {
    EXPECT_EQ(Theme::colorFromName("window"), Theme::Color::Window);
    EXPECT_EQ(Theme::colorFromName("accentText"), Theme::Color::AccentText);
    EXPECT_EQ(Theme::colorFromName("informationText"), Theme::Color::InformationText);
    EXPECT_EQ(Theme::metricFromName("pageIndicatorSize"), Theme::Metric::PageIndicatorSize);
    EXPECT_EQ(Theme::fontFromName("monospace"), Theme::Font::Monospace);
    EXPECT_EQ(Theme::surfaceFromName("buttonPrimaryPressed"), Theme::Surface::ButtonPrimaryPressed);
    EXPECT_EQ(Theme::surfaceFromName("slotHighlighted"), Theme::Surface::SlotHighlighted);
    EXPECT_FALSE(Theme::colorFromName("purple").has_value());
    EXPECT_FALSE(Theme::metricFromName("width").has_value());
    EXPECT_FALSE(Theme::fontFromName("huge").has_value());
    EXPECT_FALSE(Theme::surfaceFromName("floor").has_value());
}

TEST(ThemeTest, ShipsDarkAndLightThemes) {
    const Theme dark = Theme::dark();
    const Theme light = Theme::light();
    EXPECT_EQ(dark.getName(), "dark");
    EXPECT_EQ(light.getName(), "light");
    EXPECT_NE(dark.getColor(Theme::Color::Window), light.getColor(Theme::Color::Window));
    EXPECT_EQ(dark.getMetric(Theme::Metric::ControlHeight), light.getMetric(Theme::Metric::ControlHeight));
    EXPECT_EQ(dark.getFont(Theme::Font::Title).size, 56.0F);
    EXPECT_EQ(dark.getFont(Theme::Font::Body).font, "default");
    EXPECT_EQ(dark.getSurface(Theme::Surface::Button), nullptr);
}

TEST(ThemeTest, ReadsThemesOnTopOfABase) {
    test::EngineFixture fixture;
    Loader loader{.texture = fixture.engine().getGraphics().createTexture(graphics::Image(64, 32, math::Color::white())), .paths = {}, .options = {}};

    // clang-format off
    const core::Json document = core::Json::parse(R"({
        "name": "wood",
        "colors": {"accent": "#FF8B5A2B", "text": "#1B1E2B"},
        "metrics": {"controlHeight": 80, "panelPadding": 36},
        "fonts": {"title": {"font": "pixel", "size": 72}, "body": {"size": 34}},
        "fontFiles": {"pixel": "fonts/pixel.ttf"},
        "surfaces": {
            "panel": {"image": "ui/panel.png", "slice": [8, 12], "scale": 2, "padding": [4, 6, 8, 10], "tint": "#80FFFFFF", "filter": "linear", "fill": "tile"},
            "button": {"image": "ui/button.png", "source": [0, 0, 32, 32], "slice": 6},
            "trackFill": {"image": "ui/fill.png", "slice": [0, 4], "colorize": true},
            "badge": {"image": "ui/badge.png", "pieces": [[0,0,4,4],[4,0,4,4],[8,0,4,4],[0,4,4,4],[4,4,4,4],[8,4,4,4],[0,8,4,4],[4,8,4,4],[8,8,4,4]]},
            "card": null
        }
    })");
    // clang-format on
    const Theme theme = Theme::fromJson(document, Theme::light(), loader.function());

    EXPECT_EQ(theme.getName(), "wood");
    EXPECT_EQ(theme.getColor(Theme::Color::Accent), *math::Color::parse("#FF8B5A2B"));
    EXPECT_EQ(theme.getColor(Theme::Color::Text), *math::Color::parse("#1B1E2B"));
    EXPECT_EQ(theme.getColor(Theme::Color::Window), Theme::light().getColor(Theme::Color::Window));
    EXPECT_EQ(theme.getMetric(Theme::Metric::ControlHeight), 80.0F);
    EXPECT_EQ(theme.getFont(Theme::Font::Title).font, "pixel");
    EXPECT_EQ(theme.getFont(Theme::Font::Title).size, 72.0F);
    EXPECT_EQ(theme.getFont(Theme::Font::Body).font, "default");
    EXPECT_EQ(theme.getFont(Theme::Font::Body).size, 34.0F);
    EXPECT_EQ(theme.getFontFiles().at("pixel"), "fonts/pixel.ttf");

    const Theme::Image* panel = theme.getSurface(Theme::Surface::Panel);
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->scale, 2.0F);
    EXPECT_EQ(panel->padding, (math::Insets{.left = 10.0F, .top = 4.0F, .right = 6.0F, .bottom = 8.0F}));
    EXPECT_EQ(panel->slice.getBorders(), (math::Insets{.left = 12.0F, .top = 8.0F, .right = 12.0F, .bottom = 8.0F}));
    EXPECT_EQ(panel->slice.fill, graphics2d::NineSlice::Fill::Tile);
    EXPECT_FLOAT_EQ(panel->tint.a, 128.0F / 255.0F);
    EXPECT_FALSE(panel->colorize);
    EXPECT_TRUE(theme.getSurface(Theme::Surface::TrackFill)->colorize);
    EXPECT_EQ(theme.getSurface(Theme::Surface::Button)->slice.pieces[4], (math::Rect{6.0F, 6.0F, 20.0F, 20.0F}));
    EXPECT_EQ(theme.getSurface(Theme::Surface::Badge)->slice.pieces[8], (math::Rect{8.0F, 8.0F, 4.0F, 4.0F}));
    EXPECT_EQ(theme.getSurface(Theme::Surface::Card), nullptr);
    EXPECT_EQ(loader.paths, (std::vector<std::string>{"ui/badge.png", "ui/button.png", "ui/panel.png", "ui/fill.png"}));
    EXPECT_EQ(loader.options[2].filter, graphics::Texture::Filter::Linear);
    EXPECT_EQ(loader.options[0].filter, graphics::Texture::Filter::Nearest);
}

TEST(ThemeTest, RejectsBrokenThemes) {
    test::EngineFixture fixture;
    Loader loader{.texture = fixture.engine().getGraphics().createTexture(graphics::Image(16, 16, math::Color::white())), .paths = {}, .options = {}};
    const auto read = [&](const std::string& text) { return Theme::fromJson(core::Json::parse(text), Theme::dark(), loader.function()); };

    EXPECT_THROW(read(R"([1])"), std::invalid_argument);
    EXPECT_THROW(read(R"({"colors": {}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "shadows": {}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "colors": {"purple": "#FFFFFF"}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "colors": {"text": "white"}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "colors": []})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "metrics": {"controlHeight": -1}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "metrics": {"height": 3}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "fonts": {"huge": {"size": 3}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "fonts": {"body": {"size": 0}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "fonts": {"body": {"font": 7}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "fontFiles": {"a": 3}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "surfaces": {"floor": {"image": "a.png"}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "surfaces": {"panel": {"slice": 4}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "surfaces": {"panel": {"image": "a.png", "slice": [1, 2, 3]}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "surfaces": {"panel": {"image": "a.png", "pieces": [[0, 0, 1, 1]]}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "surfaces": {"panel": {"image": "a.png", "filter": "blurry"}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "surfaces": {"panel": {"image": "a.png", "fill": "repeat"}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "surfaces": {"panel": {"image": "a.png", "colorize": "yes"}}})"), std::invalid_argument);
    EXPECT_THROW(read(R"({"name": "x", "surfaces": {"panel": {"image": "a.png", "source": [0, 0, 1]}}})"), std::invalid_argument);
    EXPECT_THROW((void)Theme::fromJson(core::Json::parse(R"({"name": "x", "surfaces": {"panel": {"image": "a.png"}}})"), Theme::dark(), {}), std::invalid_argument);

    // Tiles smaller than a unit would repeat without end, while a piece without area draws nothing.
    try {
        (void)read(R"({"name": "x", "surfaces": {"panel": {"image": "a.png", "slice": 4, "fill": "tile", "scale": 0.0001}}})");
        ADD_FAILURE() << "tiny tiles were accepted";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The tiled edges and center of the theme surface 'panel' must each be at least 1 unit wide and tall at its scale.");
    }
    EXPECT_THROW(read(R"({"name": "x", "surfaces": {"panel": {"image": "a.png", "pieces": [[0,0,4,4],[4,0,0.5,4],[8,0,4,4],[0,4,4,4],[4,4,4,4],[8,4,4,4],[0,8,4,4],[4,8,4,4],[8,8,4,4]], "fill": "tile"}}})"), std::invalid_argument);
    EXPECT_NO_THROW(read(R"({"name": "x", "surfaces": {"panel": {"image": "a.png", "slice": [0, 4], "fill": "tile", "scale": 0.5}}})"));

    Theme theme = Theme::dark();
    EXPECT_THROW(theme.setMetric(Theme::Metric::ItemSpacing, -2.0F), std::invalid_argument);
    EXPECT_THROW(theme.setFont(Theme::Font::Body, {.font = "", .size = 10.0F}), std::invalid_argument);
    EXPECT_THROW(theme.setFont(Theme::Font::Body, {.font = "default", .size = 0.0F}), std::invalid_argument);
}

TEST(ThemeTest, SyncsTheImGuiStyle) {
    ImGuiContext* context = ImGui::CreateContext();
    const Theme theme = Theme::light();
    ImGuiStyle style;
    theme.applyTo(style);
    const math::Color text = theme.getColor(Theme::Color::Text);
    EXPECT_EQ(style.Colors[ImGuiCol_Text].x, text.r);
    EXPECT_EQ(style.Colors[ImGuiCol_Text].w, text.a);
    EXPECT_EQ(style.Colors[ImGuiCol_CheckMark].z, theme.getColor(Theme::Color::Accent).b);
    EXPECT_EQ(style.FontSizeBase, theme.getFont(Theme::Font::Body).size);
    EXPECT_EQ(style.WindowRounding, theme.getMetric(Theme::Metric::ControlRadius));
    EXPECT_EQ(style.ScrollbarSize, theme.getMetric(Theme::Metric::ScrollbarSize));
    EXPECT_EQ(style.InputTextCursorSize, theme.getMetric(Theme::Metric::CaretWidth));
    ImGui::DestroyContext(context);
}

} // namespace haylen::ui
