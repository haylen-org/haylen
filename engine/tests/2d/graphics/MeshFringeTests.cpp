#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "2d/graphics/MeshFringe.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

namespace {

// Builds the fringes of meshes one pixel to the unit.
class MeshFringeTest : public ::testing::Test {
  protected:
    [[nodiscard]] static GpuVertex vertex(float x, float y) {
        return {{x, y}, {0.0F, 0.0F}, 0xFFFFFFFFU};
    }

    MeshFringe fringe;
};

} // namespace

// The outline of a mesh moves half a pixel in and a fringe fades from it to nothing half a pixel out, while an edge that two triangles share, even through vertices of their own at the same place, keeps no fringe.
TEST_F(MeshFringeTest, FadesTheOutlineOverOnePixel) {
    const std::array<GpuVertex, 4> square{vertex(0.0F, 0.0F), vertex(10.0F, 0.0F), vertex(10.0F, 10.0F), vertex(0.0F, 10.0F)};
    const std::array<std::uint32_t, 6> triangles{0, 1, 2, 0, 2, 3};
    fringe.build(square, triangles, 1.0F);
    ASSERT_EQ(fringe.getVertices().size(), 4U + 4U * 2U);
    EXPECT_EQ(fringe.getIndices().size(), 6U + 4U * 6U);
    EXPECT_FLOAT_EQ(fringe.getVertices()[0].position[0], 0.5F);
    EXPECT_FLOAT_EQ(fringe.getVertices()[2].position[1], 9.5F);
    for (std::size_t index = 4; index < fringe.getVertices().size(); ++index) {
        const GpuVertex& outer = fringe.getVertices()[index];
        EXPECT_EQ(outer.color >> 24U, 0U);
        EXPECT_TRUE(outer.position[0] == -0.5F || outer.position[0] == 10.5F) << index;
        EXPECT_TRUE(outer.position[1] == -0.5F || outer.position[1] == 10.5F) << index;
    }

    const std::array<GpuVertex, 6> apart{vertex(0.0F, 0.0F), vertex(10.0F, 0.0F), vertex(10.0F, 10.0F), vertex(0.0F, 0.0F), vertex(10.0F, 10.0F), vertex(0.0F, 10.0F)};
    const std::array<std::uint32_t, 6> separate{0, 1, 2, 3, 4, 5};
    fringe.build(apart, separate, 1.0F);
    EXPECT_EQ(fringe.getVertices().size(), 6U + 4U * 2U);
}

// A mesh drawn by the renderer fades its outline unless the draw keeps its edges hard, and drawing it again allocates nothing.
TEST_F(MeshFringeTest, DrawsMeshesWithFadedOutlines) {
    test::EngineFixture fixture;
    const std::vector<MeshVertex> quad{{.position = {0.0F, 0.0F}}, {.position = {10.0F, 0.0F}}, {.position = {10.0F, 10.0F}}, {.position = {0.0F, 10.0F}}};
    const std::vector<std::uint32_t> triangles{0, 1, 2, 0, 2, 3};
    // clang-format off
    const auto render = [&](bool fadeEdges) {
        return test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
            Renderer& renderer = engine.getRenderer2D();
            renderer.beginWorld(Camera{});
            renderer.drawMesh({}, quad, triangles, {}, fadeEdges);
        }).vertices;
    };
    // clang-format on
    EXPECT_EQ(render(true), 12U);
    EXPECT_EQ(render(false), 4U);

    // Lua turns the fade off with the option "fadeEdges" of a draw of triangles and of a kept mesh.
    // clang-format off
    fixture.runLua(R"(
        graphics2d = require('haylen.graphics2d')
        local vertices = {{x = 0, y = 0}, {x = 10, y = 0}, {x = 10, y = 10}, {x = 0, y = 10}}
        local kept = graphics2d.newMesh(nil, vertices, {1, 2, 3, 1, 3, 4})
        require('haylen.scene').push({render = function()
            graphics2d.beginWorld(graphics2d.newCamera())
            graphics2d.drawMesh(nil, vertices, {1, 2, 3, 1, 3, 4}, {fadeEdges = false, layer = 1})
            kept:draw(20, 0, {fadeEdges = false})
            kept:draw(40, 0)
        end})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return graphics2d.stats().vertices"), "20");
}

} // namespace haylen::graphics2d
