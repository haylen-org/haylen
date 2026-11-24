#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/FrameRenderer.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::graphics2d {

// A shape with holes draws as one mesh from Lua, which takes the outlines of the polygon module or one outline, and a shape drawn every frame allocates nothing once its storage has grown.
TEST(PolygonDrawTest, DrawsShapesWithHolesFromLuaWithoutAllocating) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local graphics2d = require('haylen.graphics2d')
        local m = require('haylen.math')
        local scene = require('haylen.scene')
        local ring = m.polygon.subtract({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {{30, 30}, {30, 70}, {70, 70}, {70, 30}})
        scene.push({render = function()
            graphics2d.beginScreen()
            graphics2d.drawPolygon(ring, '#FFE0A030')
            graphics2d.drawPolygon({{200, 100}, {300, 300}, {200, 220}, {100, 300}}, '#FFE0A030')
        end})
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().drawCalls, 1U);
    EXPECT_EQ(fixture.lua("local m = require('haylen.math') return #m.triangulate(m.polygon.subtract({{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {{30, 30}, {30, 70}, {70, 70}, {70, 30}}))"), "24");

    const std::vector<std::vector<haylen::math::Vec2>> shape{{{0.0F, 0.0F}, {50.0F, 0.0F}, {50.0F, 50.0F}, {0.0F, 50.0F}}, {{10.0F, 10.0F}, {10.0F, 40.0F}, {40.0F, 40.0F}, {40.0F, 10.0F}}};
    const std::vector<haylen::math::Vec2> points{{0.0F, 0.0F}, {50.0F, 0.0F}, {50.0F, 50.0F}, {0.0F, 50.0F}, {10.0F, 10.0F}, {10.0F, 40.0F}, {40.0F, 40.0F}, {40.0F, 10.0F}};
    const std::array<std::uint32_t, 2> counts{4, 4};
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&shape, &points, &counts](haylen::core::Engine& engine) {
        engine.getRenderer2D().beginScreen();
        engine.getRenderer2D().drawPolygon(shape, haylen::math::Color::white());
        engine.getRenderer2D().drawPolygon(points, counts, haylen::math::Color::white());
    }));
    // clang-format on
    fixture.frames(3);
    const test::AllocationTracker tracker;
    fixture.frames(3);
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    // The outlines of a shape in flat storage count every point it has.
    const std::array<std::uint32_t, 2> missing{4, 3};
    EXPECT_THROW(fixture.engine().getRenderer2D().drawPolygon(points, missing, haylen::math::Color::white()), std::invalid_argument);
}

} // namespace haylen::lua::graphics2d
