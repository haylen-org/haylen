#include <gtest/gtest.h>

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Gui.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::ui {

namespace {

// What the GUIs drew in the last frame: the box around every vertex and shape and the strongest value of two color channels.
struct Drawn {
    math::Rect box;
    std::uint32_t alpha = 0;
    std::uint32_t green = 0;
};

class UiTransformTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Drawn getDrawn(plugins::UiPlugin& plugin) {
        plugin.getBackend().makeCurrent();
        const ImDrawData& data = *ImGui::GetDrawData();
        math::Vec2 lowest{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        math::Vec2 highest{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
        Drawn drawn;
        // clang-format off
        const auto add = [&](math::Vec2 low, math::Vec2 high, std::uint32_t color) {
            lowest = math::Vec2::min(lowest, low);
            highest = math::Vec2::max(highest, high);
            drawn.alpha = std::max(drawn.alpha, (color >> IM_COL32_A_SHIFT) & 0xFFU);
            drawn.green = std::max(drawn.green, (color >> IM_COL32_G_SHIFT) & 0xFFU);
        };
        // clang-format on
        for (const ImDrawList* list : data.CmdLists) {
            for (const ImDrawVert& vertex : list->VtxBuffer) {
                add({vertex.pos.x, vertex.pos.y}, {vertex.pos.x, vertex.pos.y}, vertex.col);
            }
        }
        for (const graphics2d::Shape& shape : plugin.getBackend().getShapes()) {
            for (const math::Color color : {shape.color, shape.borderColor}) {
                add(shape.bounds.getMin(), shape.bounds.getMax(), color.toRgba8());
            }
        }
        drawn.box = math::Rect::fromMinMax(lowest, highest);
        return drawn;
    }
};

} // namespace

// The offset moves the node where input finds it, while scale, opacity and tint reshape only what it drew.
TEST_F(UiTransformTest, MovesScalesFadesAndTintsANode) {
    test::EngineFixture fixture;
    plugins::UiPlugin& plugin = fixture.engine().getPlugin<plugins::UiPlugin>();
    const std::shared_ptr<Gui> gui = plugin.createGui({{"kind", "button"}, {"id", "title"}, {"text", "Hello there"}}, Placement::Screen);
    plugin.mount(gui, 0);
    fixture.frames(2);
    const math::Rect plain = gui->find("title")->getBounds();
    const Drawn before = getDrawn(plugin);
    EXPECT_EQ(before.alpha, 255U);

    Transform& shape = *gui->find("title")->getTransform();
    shape.offset = {30.0F, 10.0F};
    shape.scale = {2.0F, 2.0F};
    shape.opacity = 0.5F;
    shape.tint = {1.0F, 0.0F, 0.0F, 1.0F};
    fixture.frames(1);
    const math::Rect moved = gui->find("title")->getBounds();
    EXPECT_FLOAT_EQ(moved.x, plain.x + 30.0F);
    EXPECT_FLOAT_EQ(moved.y, plain.y + 10.0F);

    const Drawn after = getDrawn(plugin);
    EXPECT_NEAR(after.box.width, before.box.width * 2.0F, 1.0F);
    EXPECT_NEAR(after.box.getCenter().x, before.box.getCenter().x + 30.0F, 1.0F);
    EXPECT_NEAR(after.box.getCenter().y, before.box.getCenter().y + 10.0F, 1.0F);
    EXPECT_EQ(after.alpha, 128U);
    EXPECT_EQ(after.green, 0U);
}

TEST_F(UiTransformTest, TweensNodesNativelyFromLua) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        ui = require('haylen.ui')
        tween = require('haylen.tween')
        m = require('haylen.math')
        doc = ui.mount(ui.column{id = 'menu', ui.label{id = 'title', text = 'Hello'}}, {placement = 'screen'})
        title = doc:transform('title')
        same = rawequal(title, doc:transform('title'))
        tween.to(doc:transform('title'), 1, {opacity = 0, offset = m.vec2(40, 0), ['scale.x'] = 3, tint = '#FF0000'})
    )");
    // clang-format on
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return string.format('%.2f %.1f %.1f %.2f %s', title.opacity, title.offset.x, title.scale.x, title.tint.g, tostring(same))"), "0.50 20.0 2.0 0.50 true");
    EXPECT_EQ(fixture.lua("title.opacity = 0.25 return title.opacity"), "0.25");

    // Replacing the node releases its handle, and the tween stops before it writes again.
    fixture.runLua("doc:replaceChildren('menu', {ui.label{id = 'title', text = 'Again'}})");
    EXPECT_NE(fixture.lua("return title.opacity").find("haylen.UiTransform\" was already released."), std::string::npos);
    EXPECT_EQ(fixture.lua("return tostring(rawequal(title, doc:transform('title'))) .. ' ' .. doc:transform('title').opacity"), "false 1.0");
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return tween.size()"), "0");
    EXPECT_NE(fixture.lua("doc:transform('missing')").find("no node with the id \"missing\""), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::ui
