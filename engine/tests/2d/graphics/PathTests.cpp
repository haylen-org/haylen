#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "2d/graphics/Canvas.hpp"
#include "2d/graphics/PathTessellation.hpp"
#include "2d/graphics/PolygonMesh.hpp"
#include "2d/graphics/RendererState.hpp"
#include "2d/graphics/StrokeBuilder.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/graphics/Path.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Transform2D.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::graphics2d {

namespace {

// Reads the coverage that the triangles of a path give a point, from 0 outside to 255 inside, as the GPU interpolates the alpha of their vertices.
class PathCoverageTest : public ::testing::Test {
  protected:
    [[nodiscard]] static float coverage(const PathTessellation::Triangles& triangles, math::Vec2 point) {
        float found = 0.0F;
        for (std::size_t index = 0; index + 2 < triangles.indices.size(); index += 3) {
            const GpuVertex& a = triangles.vertices[triangles.indices[index]];
            const GpuVertex& b = triangles.vertices[triangles.indices[index + 1]];
            const GpuVertex& c = triangles.vertices[triangles.indices[index + 2]];
            const math::Vec2 pa{a.position[0], a.position[1]};
            const math::Vec2 pb{b.position[0], b.position[1]};
            const math::Vec2 pc{c.position[0], c.position[1]};
            const float area = math::Vec2::cross(pb - pa, pc - pa);
            if (std::fabs(area) < 1.0e-9F) {
                continue;
            }
            const float u = math::Vec2::cross(pb - point, pc - point) / area;
            const float v = math::Vec2::cross(pc - point, pa - point) / area;
            const float w = 1.0F - u - v;
            if (u >= -1.0e-5F && v >= -1.0e-5F && w >= -1.0e-5F) {
                found = std::max(found, u * static_cast<float>(a.color >> 24U) + v * static_cast<float>(b.color >> 24U) + w * static_cast<float>(c.color >> 24U));
            }
        }
        return found;
    }

    // A star of five points drawn in one outline that crosses itself, whose middle the outline winds around twice.
    [[nodiscard]] static Path star() {
        Path path;
        for (int corner = 0; corner < 5; ++corner) {
            const float angle = -math::Math::kHalfPi + static_cast<float>(corner * 2) * math::Math::kTau / 5.0F;
            const math::Vec2 point = math::Vec2{std::cos(angle), std::sin(angle)} * 100.0F;
            if (corner == 0) {
                path.moveTo(point);
            } else {
                path.lineTo(point);
            }
        }
        path.close();
        return path;
    }

    // Adds a closed square and an open curve beside it.
    static void addSquareAndCurve(Path& path) {
        path.moveTo({0.0F, 0.0F});
        path.lineTo({10.0F, 0.0F});
        path.lineTo({10.0F, 10.0F});
        path.lineTo({0.0F, 10.0F});
        path.close();
        path.moveTo({20.0F, 0.0F});
        path.quadraticTo({30.0F, 10.0F}, {40.0F, 0.0F});
    }

    PolygonMesh polygons;
    StrokeBuilder strokes;
};

} // namespace

// Every line of a curve or an arc stays within the tolerance of it, and the outline ends on the exact end of the command.
TEST(PathCommandTest, FlattensCurvesAndArcsWithinTheTolerance) {
    Path path;
    path.arc({0.0F, 0.0F}, 100.0F, 0.0F, math::Math::kTau);
    path.close();
    Path::Outlines outlines;
    path.flatten(0.25F, outlines);
    ASSERT_EQ(outlines.counts.size(), 1U);
    EXPECT_EQ(outlines.closed[0], 1U);
    EXPECT_GT(outlines.counts[0], 30U);
    EXPECT_LT(outlines.counts[0], 80U);
    for (std::size_t index = 0; index < outlines.points.size(); ++index) {
        const math::Vec2 point = outlines.points[index];
        const math::Vec2 next = outlines.points[(index + 1) % outlines.points.size()];
        EXPECT_NEAR(point.getLength(), 100.0F, 0.001F);
        EXPECT_LE(100.0F - ((point + next) * 0.5F).getLength(), 0.25F);
    }

    // A positive sweep turns clockwise on the screen, where y grows downward, from the x axis toward the y axis.
    path.clear();
    path.arc({0.0F, 0.0F}, 10.0F, 0.0F, math::Math::kHalfPi);
    path.flatten(0.25F, outlines);
    EXPECT_NEAR(outlines.points.back().x, 0.0F, 0.0001F);
    EXPECT_NEAR(outlines.points.back().y, 10.0F, 0.0001F);

    const math::Vec2 start{0.0F, 0.0F};
    const math::Vec2 first{30.0F, 120.0F};
    const math::Vec2 second{170.0F, -80.0F};
    const math::Vec2 end{200.0F, 40.0F};
    path.clear();
    path.moveTo(start);
    path.cubicTo(first, second, end);
    path.flatten(0.25F, outlines);
    ASSERT_EQ(outlines.counts.size(), 1U);
    EXPECT_EQ(outlines.closed[0], 0U);
    EXPECT_EQ(outlines.points.back(), end);
    for (int sample = 0; sample <= 1000; ++sample) {
        const float t = static_cast<float>(sample) / 1000.0F;
        const float u = 1.0F - t;
        const math::Vec2 curve = start * (u * u * u) + first * (3.0F * u * u * t) + second * (3.0F * u * t * t) + end * (t * t * t);
        float nearest = 1.0e9F;
        for (std::size_t index = 0; index + 1 < outlines.points.size(); ++index) {
            const math::Vec2 a = outlines.points[index];
            const math::Vec2 b = outlines.points[index + 1];
            const float along = std::clamp(math::Vec2::dot(curve - a, b - a) / (b - a).getLengthSquared(), 0.0F, 1.0F);
            nearest = std::min(nearest, math::Vec2::distance(curve, a + (b - a) * along));
        }
        EXPECT_LE(nearest, 0.25F) << t;
    }

    path.clear();
    path.moveTo(start);
    path.quadraticTo({100.0F, 200.0F}, {200.0F, 0.0F});
    path.flatten(0.25F, outlines);
    EXPECT_NEAR(outlines.points[outlines.points.size() / 2].y, 100.0F, 0.5F);
    EXPECT_EQ(outlines.points.back(), (math::Vec2{200.0F, 0.0F}));
}

// A line after a close starts a new outline at the start of the one that closed, an arc joins the point before it with a line, an outline without a line drops, and commands without a point to continue from or with values that cannot draw throw.
TEST(PathCommandTest, StartsOutlinesClosesThemAndRejectsMisuse) {
    Path path;
    EXPECT_THROW(path.lineTo({1.0F, 1.0F}), std::logic_error);
    EXPECT_THROW(path.close(), std::logic_error);
    EXPECT_THROW(path.moveTo({std::nanf(""), 0.0F}), std::invalid_argument);
    EXPECT_THROW(path.arc({0.0F, 0.0F}, -1.0F, 0.0F, 1.0F), std::invalid_argument);
    EXPECT_TRUE(path.empty());

    path.moveTo({0.0F, 0.0F});
    path.lineTo({10.0F, 0.0F});
    path.lineTo({10.0F, 10.0F});
    path.lineTo({0.0F, 0.0F});
    path.close();
    path.lineTo({-10.0F, 0.0F});
    path.moveTo({50.0F, 50.0F});
    path.moveTo({100.0F, 0.0F});
    path.arc({100.0F, 20.0F}, 10.0F, 0.0F, math::Math::kPi);
    Path::Outlines outlines;
    path.flatten(1.0F, outlines);

    ASSERT_EQ(outlines.counts.size(), 3U);
    EXPECT_EQ(outlines.counts[0], 3U) << "The closed outline drops the point that returns to its start.";
    EXPECT_EQ(outlines.closed[0], 1U);
    EXPECT_EQ(outlines.counts[1], 2U);
    EXPECT_EQ(outlines.points[3], (math::Vec2{0.0F, 0.0F}));
    EXPECT_EQ(outlines.closed[1], 0U);
    EXPECT_EQ(outlines.points[5], (math::Vec2{100.0F, 0.0F}));
    EXPECT_NEAR(outlines.points[6].x, 110.0F, 0.0001F);
    EXPECT_NEAR(outlines.points[6].y, 20.0F, 0.0001F);
    EXPECT_THROW(path.flatten(0.0F, outlines), std::invalid_argument);

    const Path copy = path;
    EXPECT_EQ(copy.getVerbs().size(), path.getVerbs().size());
    EXPECT_NE(&copy.getTessellation(), &path.getTessellation());
}

// The rule decides what crossings and overlaps fill: a star that crosses itself and two circles wound alike fill their middle by the nonzero rule and leave it empty by the even-odd rule, while a circle wound the other way cuts a hole by both.
TEST_F(PathCoverageTest, FillsByTheNonZeroAndTheEvenOddRules) {
    Path path = star();
    PathTessellation& tessellation = path.getTessellation();
    EXPECT_FLOAT_EQ(coverage(tessellation.getFill(path, Path::FillRule::NonZero, 1.0F, polygons), {0.0F, 0.0F}), 255.0F);
    EXPECT_FLOAT_EQ(coverage(tessellation.getFill(path, Path::FillRule::NonZero, 1.0F, polygons), {0.0F, -80.0F}), 255.0F);
    EXPECT_FLOAT_EQ(coverage(tessellation.getFill(path, Path::FillRule::EvenOdd, 1.0F, polygons), {0.0F, 0.0F}), 0.0F);
    EXPECT_FLOAT_EQ(coverage(tessellation.getFill(path, Path::FillRule::EvenOdd, 1.0F, polygons), {0.0F, -80.0F}), 255.0F);

    for (const float sweep : {math::Math::kTau, -math::Math::kTau}) {
        Path rings;
        rings.arc({0.0F, 0.0F}, 50.0F, 0.0F, math::Math::kTau);
        rings.close();
        rings.arc({0.0F, 0.0F}, 20.0F, 0.0F, sweep);
        rings.close();
        const bool opposite = sweep < 0.0F;
        EXPECT_FLOAT_EQ(coverage(rings.getTessellation().getFill(rings, Path::FillRule::NonZero, 1.0F, polygons), {0.0F, 0.0F}), opposite ? 0.0F : 255.0F) << sweep;
        EXPECT_FLOAT_EQ(coverage(rings.getTessellation().getFill(rings, Path::FillRule::EvenOdd, 1.0F, polygons), {0.0F, 0.0F}), 0.0F) << sweep;
        EXPECT_FLOAT_EQ(coverage(rings.getTessellation().getFill(rings, Path::FillRule::EvenOdd, 1.0F, polygons), {35.0F, 0.0F}), 255.0F) << sweep;
    }
}

// The fill stops half a pixel inside every edge and fades to nothing half a pixel outside it, along the outline, the holes and the edges that crossings make, at every scale.
TEST_F(PathCoverageTest, FadesEveryEdgeOverOnePixel) {
    Path square;
    square.moveTo({0.0F, 0.0F});
    square.lineTo({10.0F, 0.0F});
    square.lineTo({10.0F, 10.0F});
    square.lineTo({0.0F, 10.0F});
    square.close();
    for (const float pixel : {0.5F, 1.0F, 2.0F}) {
        const PathTessellation::Triangles& fill = square.getTessellation().getFill(square, Path::FillRule::NonZero, pixel, polygons);
        EXPECT_NEAR(coverage(fill, {5.0F, 0.0F}), 127.5F, 1.0F) << pixel;
        EXPECT_NEAR(coverage(fill, {5.0F, pixel * 0.25F}), 191.25F, 1.0F) << pixel;
        EXPECT_FLOAT_EQ(coverage(fill, {5.0F, pixel * 0.5F}), 255.0F) << pixel;
        EXPECT_FLOAT_EQ(coverage(fill, {5.0F, -pixel * 0.5F - 0.001F}), 0.0F) << pixel;
    }

    // The inner pentagon of the star is the edge of the hole that the even-odd rule cuts, and its top edge crosses the y axis at the radius times the cosine of 72 degrees.
    Path path = star();
    const float edge = -100.0F * std::cos(math::Math::kTau / 5.0F);
    const PathTessellation::Triangles& fill = path.getTessellation().getFill(path, Path::FillRule::EvenOdd, 1.0F, polygons);
    EXPECT_NEAR(coverage(fill, {0.0F, edge}), 127.5F, 2.0F);
    EXPECT_NEAR(coverage(fill, {0.0F, edge - 0.5F}), 255.0F, 2.0F) << "The fill resolves on a grid of 64 steps per pixel.";
    EXPECT_NEAR(coverage(fill, {0.0F, edge + 0.5F}), 0.0F, 2.0F);

    // The fill maps the bounds of the path onto the texture.
    for (const GpuVertex& vertex : fill.vertices) {
        EXPECT_NEAR(vertex.uv[0], (vertex.position[0] + 95.1F) / 190.2F, 0.01F);
    }
}

// Every outline strokes on its own, closed where the path closes it, and the stroke builds again only when the commands, the pixel or the look change, so drawing the same path again allocates nothing, also after its commands are given again.
TEST_F(PathCoverageTest, StrokesEveryOutlineAndKeepsItsTriangles) {
    Path path;
    addSquareAndCurve(path);
    const std::array<float, 2> dashes{4.0F, 2.0F};
    const Stroke solid{.width = 4.0F, .join = Stroke::Join::Round};
    PathTessellation& tessellation = path.getTessellation();
    const PathTessellation::Triangles& stroke = tessellation.getStroke(path, solid, 1.0F, {1.0F, 1.0F}, strokes);
    EXPECT_FLOAT_EQ(coverage(stroke, {5.0F, 0.0F}), 255.0F);
    EXPECT_FLOAT_EQ(coverage(stroke, {0.0F, 5.0F}), 255.0F) << "The closing edge strokes too.";
    EXPECT_FLOAT_EQ(coverage(stroke, {5.0F, 5.0F}), 0.0F);
    EXPECT_FLOAT_EQ(coverage(stroke, {30.0F, 5.0F}), 255.0F);
    EXPECT_FLOAT_EQ(coverage(stroke, {15.0F, 0.0F}), 0.0F) << "Two outlines never join.";

    const std::vector<GpuVertex> before = stroke.vertices;
    {
        const test::AllocationTracker tracker;
        static_cast<void>(tessellation.getStroke(path, solid, 1.0F, {1.0F, 1.0F}, strokes));
        static_cast<void>(tessellation.getFill(path, Path::FillRule::NonZero, 1.0F, polygons));
        const std::size_t built = tracker.getTotal();
        path.clear();
        addSquareAndCurve(path);
        static_cast<void>(tessellation.getFill(path, Path::FillRule::NonZero, 1.0F, polygons));
        static_cast<void>(tessellation.getStroke(path, solid, 1.0F, {1.0F, 1.0F}, strokes));
        EXPECT_EQ(tracker.getTotal(), built);
    }
    const test::AllocationTracker again;
    static_cast<void>(tessellation.getStroke(path, solid, 1.0F, {1.0F, 1.0F}, strokes));
    EXPECT_EQ(again.getTotal(), 0U);

    Stroke dashed = solid;
    dashed.dashes = dashes;
    EXPECT_GT(tessellation.getStroke(path, dashed, 1.0F, {1.0F, 1.0F}, strokes).vertices.size(), before.size());
    EXPECT_FLOAT_EQ(coverage(tessellation.getStroke(path, dashed, 1.0F, {1.0F, 1.0F}, strokes), {5.0F, 0.0F}), 0.0F) << "The gap of the dash pattern falls there.";
    EXPECT_NE(tessellation.getStroke(path, solid, 2.0F, {1.0F, 1.0F}, strokes).vertices[0].position[1], before[0].position[1]);

    Stroke perPoint = solid;
    const std::array<float, 2> widths{1.0F, 2.0F};
    perPoint.widths = widths;
    EXPECT_THROW(static_cast<void>(tessellation.getStroke(path, perPoint, 1.0F, {1.0F, 1.0F}, strokes)), std::invalid_argument);
}

// A tinted mesh takes the color with the coverage of each vertex as its alpha, lands where the transform places it and flips its rows for a texture that starts at the bottom.
TEST(PathDrawTest, TintsPlacesAndFlipsTheTrianglesOfAPath) {
    test::EngineFixture fixture;
    RendererState state{fixture.engine().getGraphics(), fixture.engine().getJobs()};
    state.pixelRect = {0.0F, 0.0F, 100.0F, 100.0F};
    state.visibleRect = {0.0F, 0.0F, 100.0F, 100.0F};
    state.openCanvas({.kind = Canvas::Kind::Screen, .viewSize = {100.0F, 100.0F}});
    const std::array<GpuVertex, 3> vertices{GpuVertex{{0.0F, 0.0F}, {0.0F, 0.25F}, 0xFFFFFFFFU}, GpuVertex{{10.0F, 0.0F}, {1.0F, 0.25F}, 0x80FFFFFFU}, GpuVertex{{0.0F, 10.0F}, {0.0F, 1.0F}, 0x00FFFFFFU}};
    const std::array<std::uint32_t, 3> indices{0, 1, 2};
    const graphics::RenderTarget target = fixture.engine().getGraphics().createRenderTarget(8, 8);
    const bool flipped = target.getTexture().getResource()->flipped;

    state.addTintedMesh(target.getTexture(), vertices, indices, math::Color{1.0F, 0.0F, 0.0F, 0.5F}, math::Transform2D::compose({50.0F, 20.0F}, 0.0F, {2.0F, 2.0F}), {});
    ASSERT_EQ(state.vertices.size(), 3U);
    EXPECT_FLOAT_EQ(state.vertices[1].position[0], 70.0F);
    EXPECT_FLOAT_EQ(state.vertices[1].position[1], 20.0F);
    EXPECT_EQ(state.vertices[0].color, 0x800000FFU);
    EXPECT_EQ(state.vertices[1].color, 0x400000FFU);
    EXPECT_EQ(state.vertices[2].color, 0x000000FFU);
    EXPECT_FLOAT_EQ(state.vertices[0].uv[1], flipped ? 0.75F : 0.25F);
    state.closeCanvas();
    state.resetFrame();
}

// A path filled and stroked every frame, also at two scales, by both rules and with two strokes, and one whose commands are given again every frame, draw without allocating once their storage has grown.
TEST(PathDrawTest, DrawsPathsEveryFrameWithoutAllocating) {
    test::EngineFixture fixture;
    Path kept;
    kept.moveTo({0.0F, 0.0F});
    kept.cubicTo({60.0F, -40.0F}, {120.0F, 80.0F}, {160.0F, 0.0F});
    kept.arc({80.0F, 40.0F}, 30.0F, 0.0F, -math::Math::kTau);
    kept.close();
    Path rebuilt;
    const std::array<float, 2> dashes{6.0F, 3.0F};
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&kept, &rebuilt, &dashes](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginScreen();
        renderer.fillPath(kept, {.color = math::Color{0.2F, 0.6F, 1.0F, 1.0F}, .rule = Path::FillRule::EvenOdd});
        renderer.strokePath(kept, {.width = 3.0F, .join = Stroke::Join::Round, .dashes = dashes}, {}, math::Transform2D::compose({200.0F, 100.0F}, 0.3F, {1.5F, 1.5F}));
        renderer.fillPath(kept, {.rule = Path::FillRule::NonZero}, {}, math::Transform2D::compose({300.0F, 100.0F}, 0.0F, {0.25F, 0.25F}));
        renderer.strokePath(kept, {.width = 1.0F});
        rebuilt.clear();
        rebuilt.moveTo({10.0F, 10.0F});
        rebuilt.quadraticTo({40.0F, 60.0F}, {80.0F, 10.0F});
        rebuilt.close();
        renderer.fillPath(rebuilt);
    }));
    // clang-format on
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().drawCalls, 1U);
    const test::AllocationTracker tracker;
    fixture.frames(3);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

// The path of Lua chains its commands, fills and strokes itself at a place with a rotation and a scale, gives its outlines as a shape and names what it cannot take.
TEST(PathLuaTest, BuildsFillsStrokesAndFlattensPaths) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        graphics2d = require('haylen.graphics2d')
        path = graphics2d.newPath()
        empty = path.empty
        path:moveTo(0, 0):lineTo(100, 0):quadraticTo(150, 50, 100, 100):cubicTo(60, 140, 40, 60, 0, 100):close()
        path:arc(50, 50, 20, 0, -2 * math.pi):close()
        require('haylen.scene').push({render = function()
            graphics2d.beginScreen()
            path:fill(10, 20, {color = '#FF3080FF', fillRule = 'evenOdd', rotation = 0.5, scaleX = 2, layer = 1})
            path:stroke(10, 20, {width = 4, join = 'round', cap = 'round', dashes = {8, 4}, color = '#FFFFFFFF', scaleY = 2, layer = 1})
        end})
    )");
    // clang-format on
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.lua("return tostring(empty) .. ' ' .. tostring(path.empty)"), "true false");
    EXPECT_EQ(fixture.lua("local shape, closed = path:outlines(1) return #shape .. ' ' .. tostring(closed[1]) .. ' ' .. shape[1][2].x .. ' ' .. tostring(#shape[2] > 8)"), "2 true 100.0 true");
    EXPECT_EQ(fixture.lua("return #graphics2d.newPath():moveTo(0, 0):lineTo(5, 5):outlines()"), "1");

    EXPECT_NE(fixture.lua("graphics2d.newPath():lineTo(1, 1)").find("A path has no point to continue from, so \"moveTo\" comes before its first line or curve."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.newPath():close()").find("A path closes only an outline it has open"), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.newPath():arc(0, 0, -1, 0, 1)").find("An arc of a path takes a finite radius of zero or more and finite angles."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.beginScreen() path:fill(0, 0, {rule = 'evenOdd'})").find("Unknown option \"rule\"."), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.beginScreen() path:fill(0, 0, {fillRule = 'odd'})").find("fillRule"), std::string::npos);
    EXPECT_NE(fixture.lua("graphics2d.beginScreen() path:stroke(0, 0, {widths = {1, 2}})").find("Unknown option \"widths\"."), std::string::npos);
    EXPECT_NE(fixture.lua("path:outlines(0)").find("A path flattens within a positive and finite tolerance."), std::string::npos);
}

} // namespace haylen::graphics2d
