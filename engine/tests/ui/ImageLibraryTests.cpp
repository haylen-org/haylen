#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/UiFixture.hpp"
#include "ui/ImGuiConverter.hpp"
#include "ui/ImageLibrary.hpp"

namespace haylen::ui {

namespace {

class ImageLibraryTest : public ::testing::Test, public test::UiFixture {
  protected:
    static constexpr std::string_view kBadge = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 12"><rect width="24" height="12" rx="6" fill="#FF3366"/><circle cx="6" cy="6" r="4" fill="currentColor"/></svg>)";
    static constexpr std::string_view kSquare = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><rect width="100" height="100" fill="#3366FF"/></svg>)";

    ImageLibraryTest() : UiFixture({{"content/icons/badge.svg", std::string(kBadge)}, {"content/icons/square.svg", std::string(kSquare)}, {"content/icons/broken.svg", "not a picture"}}) {}

    [[nodiscard]] Context& getContext() {
        return getUi().getContext();
    }

    // The pixels a picture drawn over a size in UI units covers on the screen.
    [[nodiscard]] math::Vec2 toPixels(math::Vec2 size) {
        return size * getUi().getBackend().getDensity();
    }

    // Tells whether a vertex the GUIs drew in the last frame has the color.
    [[nodiscard]] bool drawsColor(ImU32 color) {
        getUi().getBackend().makeCurrent();
        const ImVector<ImDrawVert>& vertices = ImGui::FindWindowByName("##haylen-guis")->DrawList->VtxBuffer;
        return std::any_of(vertices.begin(), vertices.end(), [color](const ImDrawVert& vertex) { return vertex.col == color; });
    }

    // Returns the box the vertices of a color the GUIs drew in the last frame cover, in UI units.
    [[nodiscard]] math::Rect findColorBox(ImU32 color) {
        getUi().getBackend().makeCurrent();
        const ImVector<ImDrawVert>& vertices = ImGui::FindWindowByName("##haylen-guis")->DrawList->VtxBuffer;
        math::Vec2 minimum{1e9F, 1e9F};
        math::Vec2 maximum{-1e9F, -1e9F};
        for (const ImDrawVert& vertex : vertices) {
            if (vertex.col == color) {
                minimum = {std::min(minimum.x, vertex.pos.x), std::min(minimum.y, vertex.pos.y)};
                maximum = {std::max(maximum.x, vertex.pos.x), std::max(maximum.y, vertex.pos.y)};
            }
        }
        return math::Rect::fromMinMax(minimum, maximum);
    }

    // Counts the vertices the GUIs drew in the last frame.
    [[nodiscard]] int countVertices() {
        getUi().getBackend().makeCurrent();
        return ImGui::FindWindowByName("##haylen-guis")->DrawList->VtxBuffer.Size;
    }
};

} // namespace

TEST_F(ImageLibraryTest, ShowsSvgDocumentsAtTheSizeTheyGiveThemselves) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "image", "id": "badge", "image": "icons/badge.svg", "scale": 2}, {"kind": "imageButton", "id": "button", "image": "icons/badge.svg"}]})");
    ASSERT_TRUE(getFixture().frameUntil([&] { return getBounds(*gui, "badge").width > 0.0F; }));
    EXPECT_EQ(getBounds(*gui, "badge").getSize(), math::Vec2(48.0F, 24.0F));
    EXPECT_EQ(getBounds(*gui, "button").getSize(), math::Vec2(24.0F, 12.0F));
    EXPECT_EQ(getContext().getImageSize("icons/badge.svg"), math::Vec2(24.0F, 12.0F));
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(ImageLibraryTest, RastersSvgDocumentsForThePixelsTheyCover) {
    const auto raster = [&](math::Vec2 size) { return getContext().getImage("icons/badge.svg", size); };
    ASSERT_TRUE(getFixture().frameUntil([&] { return raster({96.0F, 48.0F}).isValid(); }));

    // A raster covers the pixels of the draw and is at most a quarter of an octave larger.
    const math::Vec2 pixels = toPixels({96.0F, 48.0F});
    const math::Vec2 size = raster({96.0F, 48.0F}).getSize();
    EXPECT_GE(size.x, std::floor(pixels.x));
    EXPECT_LE(size.x, std::ceil(pixels.x * std::exp2(0.25F)) + 1.0F);
    EXPECT_NEAR(size.x / size.y, 2.0F, 0.1F);
    EXPECT_EQ(raster({96.0F, 48.0F}).getOptions().filter, graphics::Texture::Filter::Linear);

    // A larger draw rasters again, and the raster it has stands in until the new one arrives.
    const graphics::Texture small = raster({96.0F, 48.0F});
    EXPECT_EQ(raster({960.0F, 480.0F}), small);
    ASSERT_TRUE(getFixture().frameUntil([&] { return raster({960.0F, 480.0F}) != small; }));
    EXPECT_GE(raster({960.0F, 480.0F}).getSize().x, std::floor(toPixels({960.0F, 480.0F}).x));
    EXPECT_EQ(raster({96.0F, 48.0F}), small);
}

// A transform that scales a picture rasterizes it for the pixels it covers once scaled, so it stays sharp while it grows.
TEST_F(ImageLibraryTest, RastersPicturesForTheScaleOfTheirTransforms) {
    // clang-format off
    const auto scaled = [&] {
        getContext().pushReshape({.scale = {3.0F, 3.0F}}, {});
        const graphics::Texture texture = getContext().getImage("icons/badge.svg", {96.0F, 48.0F});
        getContext().popReshape();
        return texture;
    };
    // clang-format on
    ASSERT_TRUE(getFixture().frameUntil([&] { return scaled().getSize().x >= std::floor(toPixels({288.0F, 144.0F}).x); }));
}

TEST_F(ImageLibraryTest, DrawsSvgPicturesInEveryComponentThatShowsOne) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "icon", "image": "icons/badge.svg", "size": 48, "color": "accent"},
        {"kind": "button", "id": "cart", "text": "Cart", "icon": "icons/badge.svg", "variant": "toolbar"},
        {"kind": "avatar", "image": "icons/square.svg", "size": 64},
        {"kind": "emptyState", "image": "icons/square.svg", "title": "Nothing"},
        {"kind": "list", "items": [{"id": "one", "text": "One", "image": "icons/badge.svg"}]},
        {"kind": "slotGrid", "slots": [{"id": "slot", "image": "icons/square.svg"}]}
    ]})");
    // clang-format on
    frames(30);
    EXPECT_EQ(getEngine().getError(), nullptr);
    EXPECT_GT(getBounds(*gui, "cart").width, 0.0F);
}

TEST_F(ImageLibraryTest, RoundsTheCornersOfImages) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "image", "id": "photo", "image": "icons/square.svg", "width": 200, "height": 120, "fit": "cover"}]})");
    ASSERT_TRUE(getFixture().frameUntil([&] { return getContext().getImage("icons/square.svg", {200.0F, 200.0F}).isValid(); }));
    frames(2);
    const int square = countVertices();
    gui->set("photo", {{"radius", 24}});
    frames(2);
    EXPECT_GT(countVertices(), square);

    EXPECT_THROW(gui->set("photo", {{"radius", -1}}), std::invalid_argument);
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(ImageLibraryTest, TintsTheIconOfAButtonWithItsIconColor) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "help", "text": "Help", "icon": "icons/badge.svg"}]})");
    ASSERT_TRUE(getFixture().frameUntil([&] { return getContext().getImage("icons/badge.svg", {24.0F, 24.0F}).isValid(); }));
    const ImU32 danger = ImGuiConverter::toImU32(getUi().getTheme().getColor(Theme::Color::Danger));
    frames(2);
    EXPECT_FALSE(drawsColor(danger));

    gui->set("help", {{"iconColor", "danger"}});
    frames(2);
    EXPECT_TRUE(drawsColor(danger));
    EXPECT_THROW(gui->set("help", {{"iconColor", "crimson"}}), std::invalid_argument);
}

TEST_F(ImageLibraryTest, PlacesTheIconOfAButtonAboveItsLabelOrWithTheContentAtTheStart) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "alignItems": "start", "children": [
        {"kind": "button", "id": "tab", "text": "Home", "icon": "icons/badge.svg", "iconColor": "danger", "iconPlacement": "top"},
        {"kind": "button", "id": "search", "text": "Search", "icon": "icons/badge.svg", "iconColor": "success", "justify": "start", "width": 600}
    ]})");
    // clang-format on
    ASSERT_TRUE(getFixture().frameUntil([&] { return getContext().getImage("icons/badge.svg", {36.0F, 36.0F}).isValid(); }));
    frames(2);
    const Theme& theme = getUi().getTheme();
    const float iconSize = theme.getMetric(Theme::Metric::IconSize);

    // The icon stands above the label, in the middle of a button taller than a control.
    const math::Rect tab = getBounds(*gui, "tab");
    const math::Rect tabIcon = findColorBox(ImGuiConverter::toImU32(theme.getColor(Theme::Color::Danger)));
    EXPECT_GT(tab.height, theme.getMetric(Theme::Metric::ControlHeight));
    EXPECT_NEAR(tabIcon.getCenter().x, tab.getCenter().x, 1.0F);
    EXPECT_LT(tabIcon.y, tab.getCenter().y - iconSize * 0.5F);
    EXPECT_NEAR(tabIcon.width, iconSize, 1.0F);

    // The content of the search bar starts after the padding of the control.
    const math::Rect search = getBounds(*gui, "search");
    const math::Rect searchIcon = findColorBox(ImGuiConverter::toImU32(theme.getColor(Theme::Color::Success)));
    EXPECT_NEAR(searchIcon.x, search.x + theme.getMetric(Theme::Metric::ControlPaddingX), 1.0F);
    EXPECT_NEAR(searchIcon.getCenter().y, search.getCenter().y, 1.0F);

    gui->set("search", {{"justify", "end"}});
    frames(2);
    EXPECT_GT(findColorBox(ImGuiConverter::toImU32(theme.getColor(Theme::Color::Success))).x, search.getCenter().x);
    EXPECT_THROW(gui->set("tab", {{"iconPlacement", "bottom"}}), std::invalid_argument);
    EXPECT_THROW(gui->set("tab", {{"justify", "spaceBetween"}}), std::invalid_argument);
    EXPECT_EQ(getEngine().getError(), nullptr);
}

TEST_F(ImageLibraryTest, PaintsThemeSurfacesWithSvgDocuments) {
    getUi().setTheme(getUi().addTheme(getEngine(), core::Json::parse(R"({"name": "vector", "surfaces": {"panel": {"image": "icons/square.svg", "slice": 20, "scale": 2, "padding": 8}}})"), "dark"));
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "panel", "id": "box", "width": 400, "height": 300, "children": [{"kind": "label", "id": "inside", "text": "Inside"}]}]})");

    // The raster covers the pixels of the borders at their scale, and the frame draws with it.
    const float pixels = 2.0F * getUi().getBackend().getDensity().x;
    ASSERT_TRUE(getFixture().frameUntil([&] { return getContext().getImageAtScale("icons/square.svg", pixels).isValid(); }));
    frames(2);
    getUi().getBackend().makeCurrent();
    const ImTextureID raster = getContext().getTextureReference(getContext().getImageAtScale("icons/square.svg", pixels)).GetTexID();
    const ImVector<ImDrawCmd>& commands = ImGui::FindWindowByName("##haylen-guis")->DrawList->CmdBuffer;
    EXPECT_TRUE(std::any_of(commands.begin(), commands.end(), [raster](const ImDrawCmd& command) { return command.TexRef._TexID == raster; }));
    EXPECT_GE(getBounds(*gui, "inside").x - getBounds(*gui, "box").x, 8.0F);
    EXPECT_EQ(getFixture().lua("local panel = require('haylen.ui').themeSurface('panel') return panel.vector .. ' ' .. tostring(panel.slice ~= nil) .. ' ' .. string.format('%.2f', panel.scale * panel.slice.texture.width / 100)"), "icons/square.svg true 2.00");
    EXPECT_EQ(getEngine().getError(), nullptr);

    // Borders that do not fit inside the document stop the app once it loaded.
    getUi().setTheme(getUi().addTheme(getEngine(), core::Json::parse(R"({"name": "wide", "surfaces": {"panel": {"image": "icons/square.svg", "slice": 60}}})"), "dark"));
    ASSERT_TRUE(getFixture().frameUntil([&] { return getEngine().getError() != nullptr; }));
    EXPECT_NE(std::string_view(getEngine().getError()->what()).find("The borders of a nine-slice must fit inside its source rectangle"), std::string::npos);
}

TEST_F(ImageLibraryTest, ReportsSvgDocumentsThatFailToLoad) {
    mount(R"({"kind": "image", "image": "icons/broken.svg"})");
    ASSERT_TRUE(getFixture().frameUntil([&] { return getEngine().getError() != nullptr; }));
    EXPECT_NE(std::string_view(getEngine().getError()->what()).find("The UI image \"icons/broken.svg\" could not be loaded."), std::string::npos);
}

TEST_F(ImageLibraryTest, ReleasesTheRastersNoFrameDrewOnceTheyPassTheBudget) {
    ImageLibrary library(getEngine().getAssets(), getEngine().getGraphics(), getEngine().getJobs(), 64U * 1024U);
    const auto raster = [&](float scale) { return library.getTexture("icons/square.svg", graphics::Texture::Filter::Linear, scale); };
    ASSERT_TRUE(getFixture().frameUntil([&] { return raster(1.0F).isValid(); }));
    const std::size_t first = library.getRasterBytes();
    EXPECT_EQ(first, std::size_t{100} * 100U * 4U);

    // The larger raster starts while the first one stands in, and arrives frames later.
    library.beginFrame();
    library.beginFrame();
    (void)raster(2.0F);
    library.beginFrame();
    library.beginFrame();
    ASSERT_TRUE(getFixture().frameUntil([&] { return library.getRasterBytes() > first; }));
    EXPECT_EQ(library.getRasterBytes(), first + std::size_t{200} * 200U * 4U);

    // The raster no frame drew lately goes, and the one drawn now stays even past the budget.
    library.beginFrame();
    EXPECT_EQ(library.getRasterBytes(), std::size_t{200} * 200U * 4U);
    EXPECT_EQ(raster(1.0F).getSize().x, 200.0F);
}

TEST_F(ImageLibraryTest, ReadsSvgPicturesAndCornersFromLua) {
    // clang-format off
    EXPECT_EQ(getFixture().lua(R"(
        local ui = require('haylen.ui')
        ui.mount(ui.image{image = 'icons/badge.svg', radius = 12, fit = 'cover', width = 120, height = 60})
        return 'mounted'
    )"), "mounted");
    EXPECT_EQ(getFixture().lua(R"(
        local ui = require('haylen.ui')
        local ok, message = pcall(ui.mount, ui.image{image = 'icons/badge.svg', radius = 5000})
        return message
    )"), "The property \"radius\" of an \"image\" must be from 0 to 4096.");
    // clang-format on
}

// Images made at run time take names that every image value of the UI takes: a render target that renders every frame, a texture, an image and a vector image.
TEST_F(ImageLibraryTest, ShowsTexturesRenderTargetsAndImagesMadeAtRunTime) {
    // clang-format off
    getFixture().runLua(R"(
        ui = require('haylen.ui')
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        minimap = graphics.newRenderTarget(64, 32)
        camera = graphics2d.newCamera()
        ui.setImage('@minimap', minimap)
        ui.setImage('@swatch', graphics.newTexture(8, 8, {fill = '#FF00FF00'}))
        ui.setImage('@stamp', graphics.newImage(10, 6, '#FFFF0000'))
        ui.setImage('@badge', graphics.newVectorImage('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 12"><rect width="24" height="12" fill="#3366FF"/></svg>'))
        require('haylen.scene').push({render = function()
            graphics2d.beginTarget(minimap, camera, {clear = '#FF203040'})
            graphics2d.drawCircle(0, 0, 10, '#FFFFFFFF')
        end})
    )");
    // clang-format on
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "image", "id": "map", "image": "@minimap"}, {"kind": "icon", "id": "swatch", "image": "@swatch"}, {"kind": "image", "id": "stamp", "image": "@stamp"}, {"kind": "image", "id": "badge", "image": "@badge", "scale": 2}]})");
    ASSERT_TRUE(getFixture().frameUntil([&] { return getBounds(*gui, "badge").width > 0.0F; }));
    EXPECT_EQ(getFixture().engine().getError(), nullptr);
    EXPECT_EQ(getBounds(*gui, "map").getSize(), math::Vec2(64.0F, 32.0F));
    EXPECT_EQ(getBounds(*gui, "stamp").getSize(), math::Vec2(10.0F, 6.0F));
    EXPECT_EQ(getBounds(*gui, "badge").getSize(), math::Vec2(48.0F, 24.0F));
    EXPECT_TRUE(drawsColor(IM_COL32(255, 255, 255, 255)));

    ImageLibrary& library = getUi().getImages();
    EXPECT_EQ(library.getTexture("@swatch", graphics::Texture::Filter::Linear, 1.0F).getSize(), math::Vec2(8.0F, 8.0F));
    library.remove("@swatch");
    EXPECT_THROW((void)library.getTexture("@swatch", graphics::Texture::Filter::Linear, 1.0F), std::runtime_error);
    EXPECT_NE(getFixture().lua("ui.setImage('minimap', minimap)").find("A UI image made at run time has a name that starts with \"@\", such as \"@minimap\", not \"minimap\"."), std::string::npos);
    EXPECT_NE(getFixture().lua("ui.setImage('@x', 5)").find("haylen.Texture expected"), std::string::npos);
    getFixture().runLua("ui.removeImage('@stamp')");
    getFixture().frames(2);
    ASSERT_NE(getFixture().engine().getError(), nullptr);
    EXPECT_NE(std::string(getFixture().engine().getError()->what()).find("The UI image \"@stamp\" could not be loaded. No image of that name was set with \"ui.setImage\"."), std::string::npos);
}

} // namespace haylen::ui
