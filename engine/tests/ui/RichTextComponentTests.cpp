#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/platform/Event.hpp"
#include "support/TestFiles.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

namespace {

class RichTextComponentTest : public ::testing::Test, public test::UiFixture {
  protected:
    RichTextComponentTest() : UiFixture(imageFiles()) {}

    [[nodiscard]] static std::map<std::string, std::string> imageFiles() {
        const std::vector<std::uint8_t> image = test::TestFiles::pngImage(16, 16, 0xFFFFFFFFU);
        return {{"content/icons/coin.png", std::string(image.begin(), image.end())}};
    }

    [[nodiscard]] std::size_t drawnSprites() {
        return getEngine().getRenderer2D().getStats().sprites;
    }
};

} // namespace

TEST_F(RichTextComponentTest, ReportsLinksAndTheirHover) {
    auto gui = mount(R"({"kind": "column", "padding": 40, "children": [{"kind": "richText", "id": "story", "text": "[url=go]Go on now[/url]"}]})");
    const math::Rect bounds = getBounds(*gui, "story");
    EXPECT_GT(bounds.width, 60.0F);

    pointer(platform::Event::Type::MouseMove, bounds.getCenter());
    frames(2);
    EXPECT_EQ(findLastEvent("linkHover").value, (core::Json{{"link", "go"}, {"hovered", true}}));
    click(bounds.getCenter());
    EXPECT_EQ(findLastEvent("link").id, "story");
    EXPECT_EQ(findLastEvent("link").value, (core::Json{{"link", "go"}}));

    pointer(platform::Event::Type::MouseMove, bounds.getMin() + math::Vec2{0.0F, bounds.height + 40.0F});
    frames(2);
    EXPECT_EQ(findLastEvent("linkHover").value, (core::Json{{"link", "go"}, {"hovered", false}}));
}

TEST_F(RichTextComponentTest, FocusesEachLinkOnceAndActivatesIt) {
    auto gui = mount(R"({"kind": "column", "padding": 40, "children": [{"kind": "richText", "id": "story", "autofocus": true, "text": "[url=one]One[/url] and [url=two]Two[/url]"}]})");
    frames();
    EXPECT_TRUE(isFocused(*gui, "story"));
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("link").value, (core::Json{{"link", "one"}}));

    key(input::Key::Right);
    key(input::Key::Right);
    key(input::Key::Enter);
    EXPECT_EQ(findLastEvent("link").value, (core::Json{{"link", "two"}}));
}

TEST_F(RichTextComponentTest, TakesTheFocusBeforeItFirstDraws) {
    std::shared_ptr<Gui> gui = getUi().createGui(core::Json::parse(R"({"kind": "column", "padding": 40, "children": [{"kind": "richText", "id": "story", "text": "[url=go]Go on now[/url]"}]})"), Placement::Screen);
    getUi().mount(gui);
    gui->command(getUi().getContext(), "story", "focus", core::Json::object());
    frames(2);
    EXPECT_TRUE(isFocused(*gui, "story"));
}

TEST_F(RichTextComponentTest, DrawsThroughTheRendererAndRevealsOverTime) {
    auto gui = mount(R"({"kind": "column", "children": [{"kind": "richText", "id": "story", "text": "[b]abcd[/b] [img=icons/coin.png]", "revealSpeed": 30}]})");
    EXPECT_LT(drawnSprites(), 2U);
    frames(12);
    EXPECT_TRUE(getFixture().frameUntil([&] { return drawnSprites() == 5U; }));

    gui->set("story", core::Json{{"visibleCharacters", 2}});
    frames();
    EXPECT_EQ(drawnSprites(), 2U);

    gui->set("story", core::Json{{"text", "plain"}, {"revealSpeed", 0}, {"visibleCharacters", -1}, {"textAlign", "center"}, {"wrap", false}});
    frames();
    EXPECT_EQ(drawnSprites(), 5U);

    // Text without links never takes the focus.
    EXPECT_THROW(gui->command(getUi().getContext(), "story", "focus", core::Json::object()), std::invalid_argument);
    EXPECT_THROW(gui->set("story", core::Json{{"textAlign", "middle"}}), std::invalid_argument);
    EXPECT_THROW(gui->set("story", core::Json{{"text", "[b]open"}}), std::invalid_argument);
}

TEST_F(RichTextComponentTest, CarriesNodeTransformsToTheRenderer) {
    // A node scales around its center, and the nodes around it apply their own transform after it.
    Context& context = getUi().getContext();
    context.pushReshape({.scale = {2.0F, 2.0F}, .opacity = 0.5F}, {100.0F, 100.0F});
    context.pushReshape({.scale = {0.5F, 1.0F}, .tint = math::Color{1.0F, 0.0F, 0.0F, 1.0F}}, {10.0F, 10.0F});
    const Context::Reshape both = context.getReshape();
    EXPECT_EQ(both.scale, math::Vec2(1.0F, 2.0F));
    EXPECT_EQ(math::Vec2(10.0F, 10.0F) * both.scale + both.offset, math::Vec2(-80.0F, -80.0F));
    EXPECT_EQ(math::Vec2(30.0F, 10.0F) * both.scale + both.offset, math::Vec2(-60.0F, -80.0F));
    EXPECT_EQ(both.color, (math::Color{1.0F, 0.0F, 0.0F, 0.5F}));
    context.popReshape();
    context.popReshape();
    EXPECT_EQ(context.getReshape().scale, math::Vec2(1.0F, 1.0F));
}

} // namespace haylen::ui
