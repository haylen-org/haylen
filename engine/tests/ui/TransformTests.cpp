#include <gtest/gtest.h>

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>

#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Document.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::ui {

namespace {

// What the documents drew in the last frame: the box around every vertex and the strongest value of two color channels.
struct Drawn {
    math::Rect box;
    std::uint32_t alpha = 0;
    std::uint32_t green = 0;
};

Drawn getDrawn(plugins::UiPlugin& plugin) {
    plugin.getBackend().makeCurrent();
    const ImDrawData& data = *ImGui::GetDrawData();
    math::Vec2 lowest{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    math::Vec2 highest{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    Drawn drawn;
    for (const ImDrawList* list : data.CmdLists) {
        for (const ImDrawVert& vertex : list->VtxBuffer) {
            lowest = {std::min(lowest.x, vertex.pos.x), std::min(lowest.y, vertex.pos.y)};
            highest = {std::max(highest.x, vertex.pos.x), std::max(highest.y, vertex.pos.y)};
            drawn.alpha = std::max(drawn.alpha, (vertex.col >> IM_COL32_A_SHIFT) & 0xFFU);
            drawn.green = std::max(drawn.green, (vertex.col >> IM_COL32_G_SHIFT) & 0xFFU);
        }
    }
    drawn.box = math::Rect::fromMinMax(lowest, highest);
    return drawn;
}

} // namespace

// The offset moves the node where input finds it, while scale, opacity and tint reshape only what it drew.
TEST(UiTransformTest, MovesScalesFadesAndTintsANode) {
    test::EngineFixture fixture;
    plugins::UiPlugin& plugin = fixture.engine().getPlugin<plugins::UiPlugin>();
    const std::shared_ptr<Document> document = plugin.createDocument({{"kind", "button"}, {"id", "title"}, {"text", "Hello there"}}, Placement::Screen);
    plugin.mount(document, 0);
    fixture.frames(2);
    const math::Rect plain = document->find("title")->getBounds();
    const Drawn before = getDrawn(plugin);
    EXPECT_EQ(before.alpha, 255U);

    Transform& shape = *document->find("title")->getTransform();
    shape.offset = {30.0F, 10.0F};
    shape.scale = {2.0F, 2.0F};
    shape.opacity = 0.5F;
    shape.tint = {1.0F, 0.0F, 0.0F, 1.0F};
    fixture.frames(1);
    const math::Rect moved = document->find("title")->getBounds();
    EXPECT_FLOAT_EQ(moved.x, plain.x + 30.0F);
    EXPECT_FLOAT_EQ(moved.y, plain.y + 10.0F);

    const Drawn after = getDrawn(plugin);
    EXPECT_NEAR(after.box.width, before.box.width * 2.0F, 1.0F);
    EXPECT_NEAR(after.box.getCenter().x, before.box.getCenter().x + 30.0F, 1.0F);
    EXPECT_NEAR(after.box.getCenter().y, before.box.getCenter().y + 10.0F, 1.0F);
    EXPECT_EQ(after.alpha, 128U);
    EXPECT_EQ(after.green, 0U);
}

TEST(UiTransformTest, TweensNodesNativelyFromLua) {
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
    fixture.runLua("doc:replace('menu', {ui.label{id = 'title', text = 'Again'}})");
    EXPECT_NE(fixture.lua("return title.opacity").find("haylen.UiTransform was already released."), std::string::npos);
    EXPECT_EQ(fixture.lua("return tostring(rawequal(title, doc:transform('title'))) .. ' ' .. doc:transform('title').opacity"), "false 1.0");
    fixture.frames(2, 0.25);
    EXPECT_EQ(fixture.lua("return tween.count()"), "0");
    EXPECT_NE(fixture.lua("doc:transform('missing')").find("no node with the id missing"), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::ui
