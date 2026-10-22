#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

#include "2d/graphics/Canvas.hpp"
#include "2d/graphics/GpuInstance.hpp"
#include "2d/graphics/PolygonMesh.hpp"
#include "2d/graphics/RendererState.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Transform2D.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

namespace {

// Reads the fields of a packed shape at the offsets the vertex layout of the shape program gives them.
class ShapeRecordTest : public ::testing::Test {
  protected:
    template <typename T> [[nodiscard]] static T at(const GpuInstance& packed, std::size_t offset) {
        const auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(GpuInstance)>>(packed);
        T value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(T));
        return value;
    }

    [[nodiscard]] static float share(const GpuInstance& packed, std::size_t offset) {
        return static_cast<float>(at<std::uint16_t>(packed, offset)) / 65535.0F;
    }

    // Reads a half float of zero or more, which the vertex layout reads as a float.
    [[nodiscard]] static float half(const GpuInstance& packed, std::size_t offset) {
        const std::uint16_t bits = at<std::uint16_t>(packed, offset);
        const int exponent = (bits >> 10U) & 0x1F;
        const float mantissa = static_cast<float>(bits & 0x3FFU);
        return exponent == 0 ? std::ldexp(mantissa, -24) : std::ldexp(1.0F + mantissa / 1024.0F, exponent - 15);
    }
};

// A renderer state with a world canvas over a destination of the size, whose view scales canvas units by the zoom.
class ShapeCanvasTest : public ShapeRecordTest {
  protected:
    void open(math::Vec2 pixels, math::Vec2 view, float zoom) {
        state.pixelRect = {0.0F, 0.0F, pixels.x, pixels.y};
        state.visibleRect = {0.0F, 0.0F, view.x, view.y};
        state.openCanvas({.kind = Canvas::Kind::World, .viewSize = view, .view = math::Transform2D::scaling({zoom, zoom})});
    }

    test::EngineFixture fixture;
    RendererState state{fixture.engine().getGraphics(), fixture.engine().getJobs()};
};

} // namespace

TEST_F(ShapeRecordTest, PacksTheFieldsTheShapeProgramReads) {
    const Shape shape{
        .bounds = {10.0F, 20.0F, 40.0F, 16.0F},
        .radii = {4.0F, 8.0F, 100.0F, 0.0F},
        .rotation = 0.5F,
        .startAngle = -math::Math::kHalfPi,
        .sweep = math::Math::kPi,
        .color = math::Color::fromHex(0xFF8040C0U),
        .borderWidth = 2.0F,
        .borderColor = math::Color::fromHex(0x10203040U),
        .softness = 3.0F,
    };
    const GpuInstance packed = GpuInstance::makeShape(shape, 1.5F, false);
    EXPECT_FLOAT_EQ(at<float>(packed, 0), 30.0F);
    EXPECT_FLOAT_EQ(at<float>(packed, 4), 28.0F);
    EXPECT_FLOAT_EQ(at<float>(packed, 8), 20.0F);
    EXPECT_FLOAT_EQ(at<float>(packed, 12), 8.0F);
    EXPECT_FLOAT_EQ(at<float>(packed, 16), 0.5F);
    EXPECT_FLOAT_EQ(half(packed, 20), 3.0F);
    EXPECT_FLOAT_EQ(half(packed, 22), 1.5F);
    EXPECT_EQ(at<std::uint32_t>(packed, 24), shape.color.toRgba8());
    EXPECT_EQ(at<std::uint32_t>(packed, 28), shape.borderColor.toRgba8());

    // The border and the radii are shares of the shorter half side, so a radius past half the shorter side rounds the whole end, and the sweep and its start are shares of a full turn.
    EXPECT_NEAR(share(packed, 32), 0.25F, 0.0001F);
    EXPECT_NEAR(share(packed, 34), 0.5F, 0.0001F);
    EXPECT_NEAR(share(packed, 36), 0.75F, 0.0001F);
    EXPECT_EQ(at<std::uint16_t>(packed, 38), 0U);
    EXPECT_NEAR(share(packed, 40), 0.5F, 0.0001F);
    EXPECT_NEAR(share(packed, 42), 1.0F, 0.0001F);
    EXPECT_NEAR(share(packed, 44), 1.0F, 0.0001F);
    EXPECT_NEAR(share(packed, 46), 0.0F, 0.0001F);

    // Half floats keep tiny pixels of zoomed canvases and clamp huge ones.
    EXPECT_NEAR(half(GpuInstance::makeShape(shape, 0.0003F, false), 22), 0.0003F, 0.000001F);
    EXPECT_FLOAT_EQ(half(GpuInstance::makeShape(shape, 1.0e9F, false), 22), 65504.0F);

    // A distortion draw lowers the coverage of the fill and of the border alike.
    GpuInstance faded = packed;
    faded.fadeShape(0.5F);
    EXPECT_EQ(at<std::uint32_t>(faded, 24) >> 24U, 0x60U);
    EXPECT_EQ(at<std::uint32_t>(faded, 28) >> 24U, 0x20U);
    EXPECT_EQ(at<std::uint32_t>(faded, 24) & 0x00FFFFFFU, shape.color.toRgba8() & 0x00FFFFFFU);
}

// The last number of the form tells the shape program to measure an ellipse instead of the rounded rectangle and to turn a flipped texture upright, while the radii of an ellipse go unused.
TEST_F(ShapeRecordTest, PacksTheKindAndTheFlipOfItsTexture) {
    EXPECT_EQ(at<std::uint16_t>(GpuInstance::makeShape({.kind = Shape::Kind::Ellipse, .bounds = {0.0F, 0.0F, 40.0F, 20.0F}}, 1.0F, false), 38), 1U);
    EXPECT_EQ(at<std::uint16_t>(GpuInstance::makeShape({.bounds = {0.0F, 0.0F, 40.0F, 20.0F}}, 1.0F, true), 38), 2U);
    EXPECT_EQ(at<std::uint16_t>(GpuInstance::makeShape({.kind = Shape::Kind::Ellipse, .bounds = {0.0F, 0.0F, 40.0F, 20.0F}}, 1.0F, true), 38), 3U);
    EXPECT_EQ(Shape::kindFromName("ellipse"), Shape::Kind::Ellipse);
    EXPECT_EQ(Shape::kindName(Shape::Kind::Rectangle), "rectangle");
    EXPECT_FALSE(Shape::kindFromName("oval").has_value());
}

// The fade of an edge spans one pixel of the destination, so the quad of a shape reaches one pixel past its edge, plus half its softness, whatever the zoom of the canvas and the density of the screen.
TEST_F(ShapeCanvasTest, ReachesOnePixelPastTheEdgeAtEveryScale) {
    state.white = fixture.engine().getGraphics().getWhiteTexture();
    struct Case {
        math::Vec2 pixels;
        math::Vec2 view;
        float zoom = 1.0F;
    };
    for (const Case& scale : {Case{{1280.0F, 720.0F}, {1920.0F, 1080.0F}, 1.0F}, Case{{2560.0F, 1440.0F}, {1920.0F, 1080.0F}, 2.0F}, Case{{960.0F, 540.0F}, {960.0F, 540.0F}, 0.5F}, Case{{3200.0F, 1800.0F}, {800.0F, 450.0F}, 3.0F}}) {
        open(scale.pixels, scale.view, scale.zoom);
        const float pixelsPerUnit = scale.pixels.x / scale.view.x * scale.zoom;
        for (const float softness : {0.0F, 6.0F}) {
            state.addShape({.bounds = {0.0F, 0.0F, 10.0F, 10.0F}, .radii = {5.0F, 5.0F, 5.0F, 5.0F}, .softness = softness}, {});
            EXPECT_NEAR((half(state.instances.back(), 22) - softness * 0.5F) * pixelsPerUnit, 1.0F, 0.02F) << pixelsPerUnit;
        }
        state.closeCanvas();
    }
    EXPECT_EQ(state.items.back().program, Program::Shape);
    state.resetFrame();
}

// A polygon fills its triangles half a pixel inside its outline and fades out over a fringe that ends half a pixel outside it, so its edge spans one pixel at every scale and in either winding.
TEST(PolygonMeshTest, FadesTheEdgeOverOnePixelAtEveryScale) {
    const std::vector<math::Vec2> square{{0.0F, 0.0F}, {10.0F, 0.0F}, {10.0F, 10.0F}, {0.0F, 10.0F}};
    const std::vector<math::Vec2> reversed{square.rbegin(), square.rend()};
    const std::uint32_t color = math::Color::fromHex(0xFF336699U).toRgba8();
    const std::array<std::uint32_t, 1> four{4};
    PolygonMesh mesh;
    for (const std::vector<math::Vec2>* outline : {&square, &reversed}) {
        for (const float pixel : {0.25F, 1.0F, 3.0F}) {
            mesh.build(*outline, four, color, pixel);
            const std::vector<GpuVertex>& vertices = mesh.getVertices();
            ASSERT_EQ(vertices.size(), 8U);
            EXPECT_EQ(mesh.getIndices().size(), 6U + 4U * 6U);
            for (std::size_t corner = 0; corner < 4; ++corner) {
                const GpuVertex& inner = vertices[corner * 2];
                const GpuVertex& outer = vertices[corner * 2 + 1];
                const math::Vec2 point = (*outline)[corner];
                EXPECT_EQ(inner.color, color);
                EXPECT_EQ(outer.color, color & 0x00FFFFFFU);
                EXPECT_NEAR(std::fabs(inner.position[0] - point.x), pixel * 0.5F, 0.0001F);
                EXPECT_NEAR(std::fabs(outer.position[1] - point.y), pixel * 0.5F, 0.0001F);
                EXPECT_TRUE(inner.position[0] > 0.0F && inner.position[0] < 10.0F && inner.position[1] > 0.0F && inner.position[1] < 10.0F) << pixel;
                EXPECT_FALSE(outer.position[0] > 0.0F && outer.position[0] < 10.0F) << pixel;
            }
        }
    }

    // The corner of a spike moves its fringe no further than the miter limit allows.
    const std::vector<math::Vec2> spike{{0.0F, 0.0F}, {100.0F, 1.0F}, {0.0F, 2.0F}};
    const std::array<std::uint32_t, 1> three{3};
    mesh.build(spike, three, color, 1.0F);
    EXPECT_LE(std::hypot(mesh.getVertices()[3].position[0] - 100.0F, mesh.getVertices()[3].position[1] - 1.0F), 1.0001F);
}

// A hole fades its edge into the hole, away from the fill, and two pieces that share an edge fill up to it without a fringe, so they join without a seam while their outer edges fade.
TEST(PolygonMeshTest, FadesHolesAndJoinsPiecesWithoutASeam) {
    const std::vector<math::Vec2> holed{{0.0F, 0.0F}, {30.0F, 0.0F}, {30.0F, 30.0F}, {0.0F, 30.0F}, {10.0F, 10.0F}, {10.0F, 20.0F}, {20.0F, 20.0F}, {20.0F, 10.0F}};
    const std::array<std::uint32_t, 2> counts{4, 4};
    const std::uint32_t color = math::Color::white().toRgba8();
    PolygonMesh mesh;
    mesh.build(holed, counts, color, 2.0F);
    const std::vector<GpuVertex>& vertices = mesh.getVertices();
    ASSERT_EQ(vertices.size(), 16U);
    EXPECT_EQ(mesh.getIndices().size(), 8U * 3U + 8U * 6U);
    for (std::size_t corner = 4; corner < 8; ++corner) {
        const math::Vec2 inner{vertices[corner * 2].position[0], vertices[corner * 2].position[1]};
        const math::Vec2 outer{vertices[corner * 2 + 1].position[0], vertices[corner * 2 + 1].position[1]};
        EXPECT_TRUE(outer.x > 10.0F && outer.x < 20.0F && outer.y > 10.0F && outer.y < 20.0F) << corner;
        EXPECT_FALSE(inner.x > 10.0F && inner.x < 20.0F && inner.y > 10.0F && inner.y < 20.0F) << corner;
    }

    // The pieces meet along x = 10, where neither fades, and their corners on it slide along it instead of moving off it.
    const std::vector<math::Vec2> pieces{{0.0F, 0.0F}, {10.0F, 0.0F}, {10.0F, 10.0F}, {0.0F, 10.0F}, {10.0F, 0.0F}, {20.0F, 0.0F}, {20.0F, 10.0F}, {10.0F, 10.0F}};
    mesh.build(pieces, counts, color, 2.0F);
    EXPECT_EQ(mesh.getIndices().size(), 4U * 3U + 6U * 6U);
    for (const std::size_t corner : {1U, 2U, 4U, 7U}) {
        EXPECT_FLOAT_EQ(mesh.getVertices()[corner * 2].position[0], 10.0F) << corner;
        EXPECT_FLOAT_EQ(mesh.getVertices()[corner * 2 + 1].position[0], 10.0F) << corner;
    }
}

// A shape with holes draws as one mesh from Lua, which takes the outlines of the polygon module or one outline, and a shape drawn every frame allocates nothing once its storage has grown.
TEST(PolygonDrawTest, DrawsShapesWithHolesFromLuaWithoutAllocating) {
    test::EngineFixture fixture;
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

    const std::vector<std::vector<math::Vec2>> shape{{{0.0F, 0.0F}, {50.0F, 0.0F}, {50.0F, 50.0F}, {0.0F, 50.0F}}, {{10.0F, 10.0F}, {10.0F, 40.0F}, {40.0F, 40.0F}, {40.0F, 10.0F}}};
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&shape](core::Engine& engine) {
        engine.getRenderer2D().beginScreen();
        engine.getRenderer2D().drawPolygon(shape, math::Color::white());
    }));
    // clang-format on
    fixture.frames(3);
    const test::AllocationTracker tracker;
    fixture.frames(3);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

// Circles, rings, arcs and shapes draw as one quad each of the shape program, which merge into one call and upload no vertices, while sprites count only the sprites.
TEST(ShapeDrawTest, DrawsEveryCurveAsOneQuadOfOneCall) {
    test::EngineFixture fixture;
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginScreen();
        renderer.drawCircle({50.0F, 50.0F}, 400.0F, math::Color::white());
        renderer.drawCircle({50.0F, 50.0F}, 2.0F, math::Color::white());
        renderer.drawRing({50.0F, 50.0F}, 20.0F, 4.0F, math::Color::white());
        renderer.drawArc({50.0F, 50.0F}, 20.0F, 4.0F, 0.0F, 1.0F, math::Color::white());
        renderer.drawShape({.bounds = {0.0F, 0.0F, 120.0F, 40.0F}, .radii = {8.0F, 8.0F, 0.0F, 0.0F}, .rotation = 0.3F, .borderWidth = 2.0F, .borderColor = math::Color::black(), .softness = 4.0F});
        renderer.drawEllipse({50.0F, 50.0F}, {30.0F, 10.0F}, math::Color::white());
        renderer.drawEllipse({50.0F, 50.0F}, {30.0F, 0.0F}, math::Color::white());
        renderer.drawCircle({50.0F, 50.0F}, 0.0F, math::Color::white());
        renderer.drawRing({50.0F, 50.0F}, 20.0F, 0.0F, math::Color::white());
        renderer.drawShape({.bounds = {0.0F, 0.0F, 0.0F, 40.0F}});
    });
    // clang-format on
    EXPECT_EQ(stats.instances, 6U);
    EXPECT_EQ(stats.vertices, 0U);
    EXPECT_EQ(stats.sprites, 0U);
    EXPECT_EQ(stats.drawCalls, 1U);

    // Points draw as one shape quad each, one color for all or one for each, in the call of the other shapes.
    const std::vector<math::Vec2> stars{{10.0F, 10.0F}, {20.0F, 30.0F}, {40.0F, 5.0F}};
    const std::vector<math::Color> tones{math::Color::white(), math::Color::black(), math::Color::white()};
    // clang-format off
    const Renderer::Stats dotted = test::FrameRenderer::renderOnce(fixture, [&stars, &tones](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginScreen();
        renderer.drawCircle({50.0F, 50.0F}, 20.0F, math::Color::white());
        renderer.drawPoints(stars, 2.0F, tones);
        renderer.drawPoints(stars, 1.0F, std::span(tones).first(1));
        renderer.drawPoints(stars, 0.0F, tones);
    });
    // clang-format on
    EXPECT_EQ(dotted.instances, 7U);
    EXPECT_EQ(dotted.drawCalls, 1U);

    // A shape with a texture of its own draws in a call of that texture, after the shapes of the white texture.
    const graphics::Texture portrait = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    // clang-format off
    const Renderer::Stats textured = test::FrameRenderer::renderOnce(fixture, [&portrait](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginScreen();
        renderer.drawCircle({50.0F, 50.0F}, 20.0F, math::Color::white());
        renderer.drawShape({.kind = Shape::Kind::Ellipse, .bounds = {0.0F, 0.0F, 40.0F, 40.0F}, .texture = portrait}, {.layer = 1});
    });
    // clang-format on
    EXPECT_EQ(textured.instances, 2U);
    EXPECT_EQ(textured.drawCalls, 2U);

    Renderer& renderer = fixture.engine().getRenderer2D();
    renderer.beginScreen();
    EXPECT_THROW(renderer.drawShape({.bounds = {0.0F, 0.0F, 10.0F, 10.0F}, .radii = {-1.0F, 0.0F, 0.0F, 0.0F}}), std::invalid_argument);
    EXPECT_THROW(renderer.drawShape({.bounds = {0.0F, 0.0F, 10.0F, 10.0F}, .borderWidth = -1.0F}), std::invalid_argument);
    EXPECT_THROW(renderer.drawShape({.bounds = {0.0F, 0.0F, 10.0F, 10.0F}, .softness = -1.0F}), std::invalid_argument);
    EXPECT_THROW(renderer.drawPoints(stars, 1.0F, std::span(tones).first(2)), std::invalid_argument);
    fixture.frames(1);
}

} // namespace haylen::graphics2d
