#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Theme.hpp"
#include "support/TestFiles.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class StyleTest : public ::testing::Test, public test::UiFixture {
  protected:
    StyleTest() : UiFixture({{"content/ui/frame.png", toText(test::TestFiles::pngImage(24, 24, 0xFFFFFFFFU))}}) {}

    [[nodiscard]] static std::string toText(const std::vector<std::uint8_t>& bytes) {
        return {bytes.begin(), bytes.end()};
    }

    // Runs a frame and tells whether the renderer drew a quad of the texture.
    [[nodiscard]] bool drawsTexture(std::string_view path) {
        bool found = false;
        graphics2d::Renderer& renderer = getEngine().getRenderer2D();
        // clang-format off
        const std::uint64_t overlay = renderer.addCanvasOverlay([&found, path](graphics2d::Renderer& drawing) {
            drawing.visitDrawn([&found, path](const graphics2d::Renderer::Drawn& drawn) {
                found = found || drawn.label == path;
            });
        });
        // clang-format on
        frames();
        renderer.removeCanvasOverlay(overlay);
        return found;
    }
};

} // namespace

TEST_F(StyleTest, OverridesTheThemeForANodeAndTheNodesInsideIt) {
    // clang-format off
    auto gui = mount(R"({"kind": "column", "children": [
        {"kind": "button", "id": "plain", "text": "Plain"},
        {"kind": "row", "id": "styled", "style": {"metrics": {"controlHeight": 100}, "fonts": {"button": {"size": 50}}, "colors": {"accent": "#FF00FF00"}}, "children": [
            {"kind": "button", "id": "inside", "text": "Plain"},
            {"kind": "column", "style": {"metrics": {"controlHeight": 40}}, "children": [{"kind": "button", "id": "deeper", "text": "Plain"}]}
        ]}
    ]})");
    // clang-format on
    EXPECT_EQ(getBounds(*gui, "plain").height, getUi().getTheme().getMetric(Theme::Metric::ControlHeight));
    EXPECT_EQ(getBounds(*gui, "inside").height, 100.0F);
    EXPECT_GT(getBounds(*gui, "inside").width, getBounds(*gui, "plain").width);
    EXPECT_EQ(getBounds(*gui, "deeper").height, 40.0F);

    // A style set again replaces the one before, and a null style goes back to the theme.
    gui->set("styled", {{"style", {{"metrics", {{"controlHeight", 80}}}}}});
    frames();
    EXPECT_EQ(getBounds(*gui, "inside").height, 80.0F);
    gui->set("styled", {{"style", nullptr}});
    frames();
    EXPECT_EQ(getBounds(*gui, "inside").height, getBounds(*gui, "plain").height);
}

TEST_F(StyleTest, DrawsASubtreeWithAnotherRegisteredTheme) {
    getUi().addTheme(getEngine(), core::Json::parse(R"({"name": "roomy", "metrics": {"controlHeight": 90}})"), "light");
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "dark", "text": "Dark"}, {"kind": "panel", "theme": "roomy", "children": [{"kind": "button", "id": "roomy", "text": "Roomy"}]}]})");
    EXPECT_EQ(getBounds(*gui, "roomy").height, 90.0F);
    EXPECT_EQ(getBounds(*gui, "dark").height, getUi().getTheme().getMetric(Theme::Metric::ControlHeight));

    // A theme that is not registered stops the app when the node draws.
    gui->set("dark", {{"theme", "missing"}});
    frames();
    ASSERT_NE(getEngine().getError(), nullptr);
    EXPECT_NE(std::string_view(getEngine().getError()->what()).find("The UI has no theme named \"missing\"."), std::string::npos);
}

TEST_F(StyleTest, PaintsTheSurfacesAStyleGivesOnceTheirImagesLoad) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "button", "id": "framed", "text": "Framed", "style": {"surfaces": {"button": {"image": "ui/frame.png", "slice": 8}}}}, {"kind": "button", "text": "Flat"}]})");
    bool painted = false;
    for (int frame = 0; frame < 60 && !painted; ++frame) {
        painted = drawsTexture("ui/frame.png");
    }
    EXPECT_TRUE(painted);

    // A style that sets a surface to flat colors paints it so, whatever the theme has.
    gui->set("framed", {{"style", {{"surfaces", {{"button", false}}}}}});
    EXPECT_FALSE(drawsTexture("ui/frame.png"));
}

TEST_F(StyleTest, ReportsTheProblemsOfAStyle) {
    // clang-format off
    const std::vector<std::pair<std::string, std::string>> cases{
        {R"({"kind": "button", "style": "big"})", R"(The property "style" of a "button" must be a table with "colors", "metrics", "fonts" or "surfaces".)"},
        {R"({"kind": "button", "style": {"sizes": {}}})", R"(Unknown key "sizes" in the property "style" of a "button".)"},
        {R"({"kind": "button", "style": {"colors": {"purple": "#FFFFFFFF"}}})", R"(The property "style" of a "button" has no color role named "purple".)"},
        {R"({"kind": "button", "style": {"colors": {"accent": "blue"}}})", R"(The color "accent" of the property "style" of a "button" must be a color such as "#FF2E7D32".)"},
        {R"({"kind": "button", "style": {"metrics": {"controlHeight": -1}}})", R"(The metric "controlHeight" of the property "style" of a "button" must be a non-negative number.)"},
        {R"({"kind": "button", "style": {"fonts": {"button": {"size": 0}}}})", R"(The size of the font "button" of the property "style" of a "button" must be positive.)"},
        {R"({"kind": "button", "style": {"fonts": {"huge": {}}}})", R"(The property "style" of a "button" has no font role named "huge".)"},
        {R"({"kind": "button", "style": {"surfaces": {"floor": false}}})", R"(The property "style" of a "button" has no surface named "floor".)"},
        {R"({"kind": "button", "style": {"surfaces": {"button": {"slice": 8}}}})", R"(The image of the surface "button" of the property "style" of a "button" must be a path.)"},
        {R"({"kind": "button", "cursor": "hand"})", R"(The property "cursor" of a "button" must be "default", "arrow", "iBeam", "crosshair", "pointingHand", "resizeHorizontal", "resizeVertical", "resizeDiagonalDown", "resizeDiagonalUp", "resizeAll" or "notAllowed".)"},
    };
    // clang-format on
    for (const auto& [node, message] : cases) {
        try {
            (void)getUi().createGui(core::Json::parse(node));
            ADD_FAILURE() << node;
        } catch (const std::invalid_argument& error) {
            EXPECT_EQ(std::string(error.what()), message);
        }
    }
}

TEST_F(StyleTest, ShowsTheCursorOfTheNodeUnderThePointer) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "label", "id": "link", "text": "Open the map", "cursor": "pointingHand"}, {"kind": "label", "id": "plain", "text": "Plain"}]})");
    pointer(platform::Event::Type::MouseMove, getBounds(*gui, "link").getCenter());
    frames(2);
    EXPECT_EQ(getFixture().host().getCursor(), platform::Window::Cursor::PointingHand);
    pointer(platform::Event::Type::MouseMove, getBounds(*gui, "plain").getCenter());
    frames(2);
    EXPECT_EQ(getFixture().host().getCursor(), platform::Window::Cursor::Default);
}

} // namespace haylen::ui
