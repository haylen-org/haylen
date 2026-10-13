#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "2d/graphics/StrokeBuilder.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Mesh.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

namespace {

// Builds strokes one pixel to the unit and reads back their sections, the four vertices each cross section adds.
class StrokeBuilderTest : public ::testing::Test {
  protected:
    void build(std::span<const math::Vec2> points, const Stroke& stroke, math::Vec2 textureSize = {1.0F, 1.0F}) {
        builder.build(points, stroke, 1.0F, textureSize, false);
    }

    [[nodiscard]] std::size_t sections() const {
        return builder.getVertices().size() / 4;
    }

    [[nodiscard]] math::Vec2 at(std::size_t section, std::size_t lane) const {
        const GpuVertex& vertex = builder.getVertices()[section * 4 + lane];
        return {vertex.position[0], vertex.position[1]};
    }

    [[nodiscard]] float alpha(std::size_t section, std::size_t lane) const {
        return static_cast<float>(builder.getVertices()[section * 4 + lane].color >> 24U) / 255.0F;
    }

    [[nodiscard]] float u(std::size_t section) const {
        return builder.getVertices()[section * 4].uv[0];
    }

    static void expectNear(math::Vec2 actual, math::Vec2 expected) {
        EXPECT_NEAR(actual.x, expected.x, 1e-3F);
        EXPECT_NEAR(actual.y, expected.y, 1e-3F);
    }

    StrokeBuilder builder;
    const std::array<math::Vec2, 2> line{{{0.0F, 0.0F}, {100.0F, 0.0F}}};
    const std::array<math::Vec2, 3> corner{{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}}};
};

} // namespace

TEST_F(StrokeBuilderTest, FadesTheEdgesAndButtEndsOverOnePixel) {
    build(line, {.width = 10.0F});
    ASSERT_EQ(sections(), 4U);
    EXPECT_EQ(builder.getIndices().size(), 3U * 18U);

    // The core stops half a pixel inside the edge and the outer vertices fade half a pixel past it, across the line and past each end.
    expectNear(at(1, 0), {0.0F, 5.5F});
    expectNear(at(1, 1), {0.0F, 4.5F});
    expectNear(at(1, 2), {0.0F, -4.5F});
    expectNear(at(1, 3), {0.0F, -5.5F});
    expectNear(at(0, 1), {-0.5F, 4.5F});
    expectNear(at(3, 1), {100.5F, 4.5F});
    EXPECT_FLOAT_EQ(alpha(1, 0), 0.0F);
    EXPECT_FLOAT_EQ(alpha(1, 1), 1.0F);
    EXPECT_FLOAT_EQ(alpha(0, 1), 0.0F);

    // A line thinner than a pixel keeps no core and fades by the share of the pixel it covers.
    build(line, {.width = 0.5F});
    expectNear(at(1, 1), {0.0F, 0.0F});
    expectNear(at(1, 0), {0.0F, 1.0F});
    EXPECT_NEAR(alpha(1, 1), 0.5F, 0.01F);
}

TEST_F(StrokeBuilderTest, ExtendsSquareCapsAndRoundsRoundCaps) {
    build(line, {.width = 10.0F, .cap = Stroke::Cap::Square});
    ASSERT_EQ(sections(), 6U);
    expectNear(at(1, 1), {-5.0F, 4.5F});
    expectNear(at(0, 1), {-5.5F, 4.5F});
    expectNear(at(5, 1), {105.5F, 4.5F});

    build(line, {.width = 10.0F, .cap = Stroke::Cap::Round});
    ASSERT_GT(sections(), 6U);
    expectNear(at(0, 0), {-5.5F, 0.0F});
    expectNear(at(0, 1), {-4.5F, 0.0F});
    expectNear(at(sections() - 1, 3), {105.5F, 0.0F});
    for (std::size_t section = 0; section < sections(); ++section) {
        EXPECT_NEAR((at(section, 0) - (section < sections() / 2 ? line[0] : line[1])).getLength(), 5.5F, 1e-3F) << section;
    }
}

TEST_F(StrokeBuilderTest, JoinsTurnsWithMitersBevelsAndArcs) {
    // A right angle mitres at the square root of two of the half width, with the inner corner on the side the stroke turns to.
    build(corner, {.width = 10.0F});
    ASSERT_EQ(sections(), 5U);
    expectNear(at(2, 1), {95.5F, 4.5F});
    expectNear(at(2, 2), {104.5F, -4.5F});
    expectNear(at(2, 3), {105.5F, -5.5F});

    build(corner, {.width = 10.0F, .join = Stroke::Join::Bevel});
    ASSERT_EQ(sections(), 6U);
    expectNear(at(2, 2), {100.0F, -4.5F});
    expectNear(at(3, 2), {104.5F, 0.0F});
    expectNear(at(2, 1), at(3, 1));

    build(corner, {.width = 10.0F, .join = Stroke::Join::Round});
    ASSERT_GT(sections(), 6U);
    for (std::size_t section = 2; section + 2 < sections(); ++section) {
        EXPECT_NEAR((at(section, 2) - corner[1]).getLength(), 4.5F, 1e-3F) << section;
    }

    // A miter longer than the limit becomes a bevel.
    const std::array<math::Vec2, 3> sharp{{{0.0F, 0.0F}, {100.0F, 0.0F}, {0.0F, 10.0F}}};
    build(sharp, {.width = 10.0F});
    EXPECT_EQ(sections(), 6U);
    build(sharp, {.width = 10.0F, .miterLimit = 100.0F});
    EXPECT_EQ(sections(), 5U);
}

TEST_F(StrokeBuilderTest, ClosesPathsWithoutASeam) {
    const std::array<math::Vec2, 5> square{{{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}, {0.0F, 0.0F}}};
    build(square, {.width = 4.0F, .closed = true});
    ASSERT_EQ(sections(), 5U);
    for (std::size_t lane = 0; lane < 4; ++lane) {
        expectNear(at(0, lane), at(4, lane));
    }
    EXPECT_FLOAT_EQ(u(0), 0.0F);
    EXPECT_FLOAT_EQ(u(4), 1.0F);
    for (std::size_t section = 0; section < sections(); ++section) {
        EXPECT_FLOAT_EQ(alpha(section, 1), 1.0F);
    }

    // Two points cannot close and draw as an open line.
    build(line, {.width = 4.0F, .closed = true});
    EXPECT_EQ(sections(), 4U);
}

TEST_F(StrokeBuilderTest, BlendsWidthsAndColorsAndRunsTexturesAlong) {
    const std::array<float, 3> widths{2.0F, 10.0F, 20.0F};
    const std::array<math::Color, 3> colors{math::Color::fromHex(0xFF0000FFU), math::Color::fromHex(0x00FF00FFU), math::Color::fromHex(0x0000FFFFU)};
    build(corner, {.widths = widths, .colors = colors});
    expectNear(at(1, 1), {0.0F, 0.5F});
    EXPECT_EQ(builder.getVertices()[1 * 4 + 1].color, colors[0].toRgba8());
    EXPECT_EQ(builder.getVertices()[2 * 4 + 1].color, colors[1].toRgba8());
    expectNear(at(3, 1), {100.0F - 9.5F, 100.0F});

    // Stretched textures span the length once, and tiled ones repeat every width times the aspect of the texture, or every given length.
    build(line, {.width = 10.0F, .textureMode = Stroke::TextureMode::Stretch});
    EXPECT_FLOAT_EQ(u(2), 1.0F);
    build(line, {.width = 10.0F, .textureMode = Stroke::TextureMode::Tile}, {32.0F, 16.0F});
    EXPECT_FLOAT_EQ(u(2), 5.0F);
    build(line, {.width = 10.0F, .textureMode = Stroke::TextureMode::Tile, .textureLength = 25.0F}, {32.0F, 16.0F});
    EXPECT_FLOAT_EQ(u(2), 4.0F);
}

TEST_F(StrokeBuilderTest, SplitsDashesAlongTheWholeLength) {
    const std::array<float, 2> pattern{10.0F, 10.0F};
    build(line, {.width = 2.0F, .dashes = pattern});
    ASSERT_EQ(sections(), 5U * 4U);
    expectNear(at(1, 1), {0.0F, 0.5F});
    expectNear(at(2, 1), {10.0F, 0.5F});
    expectNear(at(5, 1), {20.0F, 0.5F});
    EXPECT_EQ(builder.getIndices().size(), 5U * 3U * 18U);

    // An odd pattern repeats to pair up, and the offset moves into the pattern.
    const std::array<float, 1> single{10.0F};
    build(line, {.width = 2.0F, .dashes = single, .dashOffset = 15.0F});
    ASSERT_EQ(sections(), 5U * 4U);
    expectNear(at(1, 1), {5.0F, 0.5F});
    expectNear(at(sections() - 2, 1), {95.0F, 0.5F});

    // Dashes of no length draw the dots of their round caps, and nothing with butt caps.
    const std::array<float, 2> dots{0.0F, 20.0F};
    build(line, {.width = 6.0F, .cap = Stroke::Cap::Round, .dashes = dots});
    EXPECT_GT(sections(), 0U);
    expectNear((at(0, 0) + at(sections() / 5 - 1, 3)) * 0.5F, {0.0F, 0.0F});
    build(line, {.width = 6.0F, .dashes = dots});
    EXPECT_EQ(sections(), 0U);
}

TEST_F(StrokeBuilderTest, RejectsStrokesThatCannotDraw) {
    const std::array<float, 1> oneWidth{2.0F};
    EXPECT_THROW(build(line, {.widths = oneWidth}), std::invalid_argument);
    EXPECT_THROW(build(line, {.width = -1.0F}), std::invalid_argument);
    EXPECT_THROW(build(line, {.miterLimit = 0.5F}), std::invalid_argument);
    const std::array<float, 2> nothing{0.0F, 0.0F};
    EXPECT_THROW(build(line, {.dashes = nothing}), std::invalid_argument);
    const std::array<float, 2> tiny{0.0001F, 0.0001F};
    EXPECT_THROW(build(line, {.dashes = tiny}), std::invalid_argument);

    // Points that repeat leave nothing to draw.
    const std::array<math::Vec2, 2> same{{{4.0F, 4.0F}, {4.0F, 4.0F}}};
    build(same, {});
    EXPECT_EQ(sections(), 0U);
}

TEST(MeshTest, DeformsKeptVerticesAndDrawsThemWithoutAllocating) {
    Mesh grid = Mesh::grid({}, 4, 2, {10.0F, 20.0F, 40.0F, 20.0F});
    EXPECT_EQ(grid.getVertices().size(), 15U);
    EXPECT_EQ(grid.getIndices().size(), 4U * 2U * 6U);
    EXPECT_EQ(grid.getBounds(), (math::Rect{10.0F, 20.0F, 40.0F, 20.0F}));
    EXPECT_EQ(grid.getVertices()[14].uv, math::Vec2(1.0F, 1.0F));

    std::vector<float> positions(30);
    grid.readPositions(positions);
    EXPECT_FLOAT_EQ(positions[2], 20.0F);
    for (std::size_t index = 1; index < positions.size(); index += 2) {
        positions[index] += 5.0F;
    }
    grid.setPositions(positions);
    EXPECT_EQ(grid.getBounds(), (math::Rect{10.0F, 25.0F, 40.0F, 20.0F}));
    const std::array<float, 2> moved{0.0F, 0.0F};
    grid.setPositions(moved, 14);
    EXPECT_EQ(grid.getVertices()[14].position, math::Vec2(0.0F, 0.0F));
    EXPECT_THROW(grid.setPositions(moved, 15), std::out_of_range);
    EXPECT_THROW(grid.setIndices({0, 1}), std::invalid_argument);
    EXPECT_THROW(grid.setIndices({0, 1, 15}), std::invalid_argument);
    EXPECT_EQ(grid.getIndices().size(), 48U);
    EXPECT_THROW(static_cast<void>(Mesh::grid({}, 0, 2, {})), std::invalid_argument);

    test::EngineFixture fixture;
    Camera camera;
    const std::array<math::Vec2, 4> path{{{0.0F, 0.0F}, {200.0F, 40.0F}, {80.0F, 160.0F}, {300.0F, 220.0F}}};
    const std::array<float, 2> pattern{30.0F, 12.0F};
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(camera);
        engine.getRenderer2D().drawMesh(grid, {}, math::Transform2D::compose({5.0F, 5.0F}, 0.5F, {2.0F, 2.0F}));
        engine.getRenderer2D().drawStroke(path, {.width = 12.0F, .join = Stroke::Join::Round, .cap = Stroke::Cap::Round, .dashes = pattern});
    }));
    // clang-format on
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().drawCalls, 1U);
    const test::AllocationTracker tracker;
    fixture.frames(5);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

} // namespace haylen::graphics2d
