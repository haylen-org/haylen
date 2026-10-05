#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/ShaderResource.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/2d/lighting/Occluder.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/text/Font.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen {

TEST(RendererTest, BatchesSpritesByTextureAndLayer) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    const graphics::Texture first = device.createTexture(graphics::Image(16, 16, math::Color::white()));
    const graphics::Texture second = device.createTexture(graphics::Image(16, 16, math::Color::white()));

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        renderer.beginScreen();
        for (int index = 0; index < 10; ++index) {
            renderer.draw({.texture = first, .position = {static_cast<float>(index), 0.0F}});
        }
        renderer.draw({.texture = second, .order = {.layer = -1}});
        renderer.draw({.texture = first, .source = {0.0F, 0.0F, 8.0F, 8.0F}, .scale = {2.0F, 2.0F}, .flip = {.horizontal = true, .diagonal = true}, .order = {.blend = graphics::BlendMode::Type::Additive}});
    });
    // clang-format on

    EXPECT_EQ(stats.sprites, 12U);
    EXPECT_EQ(stats.instances, 12U);
    EXPECT_EQ(stats.drawCalls, 3U);
    EXPECT_EQ(stats.canvases, 1U);
}

TEST(RendererTest, SortsByDepthInsideLayers) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    const graphics::Texture other = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        graphics2d::Camera camera;
        engine.getRenderer2D().beginWorld(camera, {.sort = graphics2d::Renderer::SortMode::Depth});
        engine.getRenderer2D().draw({.texture = texture, .order = {.depth = 30.0F}});
        engine.getRenderer2D().draw({.texture = other, .order = {.depth = 20.0F}});
        engine.getRenderer2D().draw({.texture = texture, .order = {.depth = 10.0F}});
        engine.getRenderer2D().draw({.texture = other, .order = {.depth = -5.0F}});
    });
    // clang-format on

    EXPECT_EQ(stats.drawCalls, 4U);
    EXPECT_EQ(stats.textureSwitches, 4U);
}

TEST(RendererTest, DrawsPrimitivesMeshesTextAndNineSlices) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    const graphics::Texture texture = engine.getGraphics().createTexture(graphics::Image(12, 12, math::Color::white()));
    const graphics2d::NineSlice stretch = graphics2d::NineSlice::fromBorders(texture, {}, {4.0F, 4.0F, 4.0F, 4.0F});
    graphics2d::NineSlice tiled = graphics2d::NineSlice::fromBorders(texture, {0.0F, 0.0F, 12.0F, 12.0F}, math::Insets::uniform(4.0F));
    tiled.fill = graphics2d::NineSlice::Fill::Tile;
    const std::vector<math::Vec2> polygon{{0.0F, 0.0F}, {10.0F, 0.0F}, {5.0F, 3.0F}, {10.0F, 10.0F}, {0.0F, 10.0F}};

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& current) {
        graphics2d::Renderer& renderer = current.getRenderer2D();
        renderer.beginScreen();
        renderer.drawRect({0.0F, 0.0F, 10.0F, 10.0F}, math::Color::white());
        renderer.drawRectOutline({0.0F, 0.0F, 10.0F, 10.0F}, 1.0F, math::Color::white());
        renderer.drawLine({0.0F, 0.0F}, {10.0F, 10.0F}, 2.0F, math::Color::white());
        renderer.drawLine({5.0F, 5.0F}, {5.0F, 5.0F}, 2.0F, math::Color::white());
        renderer.drawPolyline(polygon, 1.0F, math::Color::white(), true);
        renderer.drawCircle({50.0F, 50.0F}, 20.0F, math::Color::white());
        renderer.drawRing({50.0F, 50.0F}, 20.0F, 4.0F, math::Color::white(), {}, 12);
        renderer.drawArc({50.0F, 50.0F}, 20.0F, 4.0F, 0.0F, 1.0F, math::Color::white());
        renderer.drawPolygon(polygon, math::Color::white());
        renderer.drawMesh({}, std::vector<graphics2d::MeshVertex>{{{0.0F, 0.0F}}, {{1.0F, 0.0F}}, {{0.0F, 1.0F}}}, std::vector<std::uint32_t>{0, 1, 2});
        renderer.drawMesh(texture, std::vector<graphics2d::MeshVertex>{}, std::vector<std::uint32_t>{});
        renderer.drawNineSlice(stretch, {0.0F, 0.0F, 100.0F, 40.0F});
        renderer.drawNineSlice(stretch, {0.0F, 0.0F, 4.0F, 4.0F});
        renderer.drawNineSlice(tiled, {0.0F, 0.0F, 30.0F, 30.0F});
        renderer.drawText(*current.getDefaultFont(), "Hello\nWorld", {10.0F, 10.0F}, {.size = 24.0F, .outlineWidth = 2.0F, .shadowOffset = {2.0F, 2.0F}, .shadowColor = math::Color::black()});
        renderer.pushClip({0.0F, 0.0F, 50.0F, 50.0F});
        renderer.pushClip({10.0F, 10.0F, 100.0F, 100.0F});
        renderer.drawRect({0.0F, 0.0F, 10.0F, 10.0F}, math::Color::white());
        renderer.popClip();
        renderer.popClip();
    });
    // clang-format on

    EXPECT_GT(stats.vertices, 0U);
    EXPECT_GT(stats.indices, 0U);
    EXPECT_GT(stats.sprites, 20U);
    EXPECT_GT(stats.uploadedBytes, 0U);
}

TEST(RendererTest, RendersLitWorldsTargetsAndStaticBatches) {
    test::EngineFixture fixture;
    fixture.host().resize({960.0F, 540.0F});
    core::Engine& engine = fixture.engine();
    const graphics::Texture texture = engine.getGraphics().createTexture(graphics::Image(8, 8, math::Color::white()));
    const graphics::RenderTarget target = engine.getGraphics().createRenderTarget(128, 64);
    std::vector<graphics2d::SpriteInstance> sprites(20000);
    for (std::size_t index = 0; index < sprites.size(); ++index) {
        sprites[index] = {.position = {static_cast<float>(index % 100), static_cast<float>(index / 100)}, .size = {4.0F, 4.0F}};
    }
    const graphics2d::StaticSpriteBatch batch = engine.getRenderer2D().createStaticBatch(texture, std::span(sprites).first(100));
    EXPECT_EQ(batch.size(), 100U);
    EXPECT_GT(batch.getBounds().width, 0.0F);
    EXPECT_EQ(batch.getTexture(), texture);

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& current) {
        graphics2d::Renderer& renderer = current.getRenderer2D();
        graphics2d::Camera camera;
        renderer.beginTarget(target, camera, {.clear = math::Color::transparent()});
        renderer.drawBatch(texture, sprites);
        renderer.drawBatch(texture, std::span<const graphics2d::SpriteInstance>{});
        renderer.drawStatic(batch);
        renderer.drawStatic(graphics2d::StaticSpriteBatch{});

        renderer.beginWorld(camera, {.ambientLight = math::Color{0.2F, 0.2F, 0.3F, 1.0F}, .postProcess = graphics2d::PostProcess{.vignetteStrength = 0.5F}});
        renderer.draw({.texture = target.getTexture()});
        renderer.drawLight({.position = {10.0F, 10.0F}, .radius = 64.0F, .intensity = 1.0F});
        renderer.drawLight({.position = {10.0F, 10.0F}, .texture = texture});

        renderer.beginWorld(camera, {.postProcess = graphics2d::PostProcess{.saturation = 0.0F}});
        renderer.draw({.texture = texture});
    });
    // clang-format on

    EXPECT_EQ(stats.lights, 2U);
    EXPECT_EQ(stats.sprites, 20000U + 100U + 2U);
    EXPECT_GE(stats.passes, 5U);
    EXPECT_EQ(graphics2d::StaticSpriteBatch{}.size(), 0U);
    EXPECT_EQ(graphics2d::StaticSpriteBatch{}.getBounds(), math::Rect{});
    EXPECT_FALSE(graphics2d::StaticSpriteBatch{}.getTexture().isValid());
}

TEST(RendererTest, RejectsInvalidUse) {
    test::EngineFixture fixture;
    graphics2d::Renderer& renderer = fixture.engine().getRenderer2D();
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(2, 2, math::Color::white()));

    EXPECT_THROW(renderer.draw({.texture = texture}), std::logic_error);
    EXPECT_THROW((void)renderer.getCanvasUnitSize(), std::logic_error);
    EXPECT_EQ(renderer.getCanvasBounds(), math::Rect{});
    renderer.beginScreen();
    EXPECT_THROW(renderer.draw({}), std::invalid_argument);
    EXPECT_THROW(renderer.drawBatch({}, std::vector<graphics2d::SpriteInstance>(1)), std::invalid_argument);
    EXPECT_THROW(renderer.drawNineSlice({}, {}), std::invalid_argument);
    EXPECT_THROW(renderer.drawNineSlice({.texture = texture, .fill = graphics2d::NineSlice::Fill::Tile}, {0.0F, 0.0F, 100.0F, 40.0F}, math::Color::white(), {}, 0.0F), std::invalid_argument);
    graphics2d::Camera shapeless;
    shapeless.viewport = math::Rect{0.0F, 0.0F, std::nanf(""), 10.0F};
    EXPECT_THROW(renderer.beginWorld(shapeless), std::invalid_argument);
    EXPECT_THROW(renderer.drawMesh(texture, std::vector<graphics2d::MeshVertex>(1), std::vector<std::uint32_t>{3}), std::out_of_range);
    EXPECT_THROW(renderer.drawLight({}), std::logic_error);
    EXPECT_THROW(renderer.popClip(), std::logic_error);
    EXPECT_THROW(renderer.beginScreen({.ambientLight = math::Color::white()}), std::invalid_argument);
    EXPECT_THROW(renderer.beginScreen({.clear = math::Color::black()}), std::invalid_argument);
    EXPECT_THROW(renderer.beginWorld(graphics2d::Camera{}, {.clear = math::Color::black()}), std::invalid_argument);
    renderer.beginWorld(graphics2d::Camera{}, {.ambientLight = math::Color::black(), .clear = math::Color::black()});
    EXPECT_THROW(renderer.drawLight({.intensity = -0.1F}), std::invalid_argument);
    renderer.drawLight({.intensity = 0.0F});
    EXPECT_THROW(renderer.beginTarget({}, graphics2d::Camera{}), std::invalid_argument);
    EXPECT_THROW(renderer.beginTarget(fixture.engine().getGraphics().createRenderTarget(4, 4), graphics2d::Camera{}, {.postProcess = graphics2d::PostProcess{.materials = {graphics2d::Material{}}}}), std::invalid_argument);
    EXPECT_THROW((void)renderer.createStaticBatch({}, std::vector<graphics2d::SpriteInstance>(1)), std::invalid_argument);
    EXPECT_TRUE(fixture.engine().getGraphics().getWhiteTexture().isValid());
    EXPECT_TRUE(renderer.getLightTexture().isValid());
    fixture.frames(1);
}

// Draws that shade and clip alike merge, even when the app made other draws between them.
TEST(RendererTest, MergesDrawsThatShadeAndClipAlike) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(graphics2d::Camera{}, {.ambientLight = math::Color::white()});
        for (int index = 0; index < 8; ++index) {
            renderer.draw({.texture = texture, .order = {.layer = 0}});
            renderer.draw({.texture = texture, .order = {.layer = 1}});
        }
        renderer.beginScreen();
        for (int index = 0; index < 8; ++index) {
            renderer.pushClip({0.0F, 0.0F, 100.0F, 100.0F});
            renderer.draw({.texture = texture});
            renderer.popClip();
        }
    });
    // clang-format on

    // One draw call per layer of the lit canvas, its composite, and one for every clipped draw of the screen.
    EXPECT_EQ(stats.drawCalls, 4U);
}

// A canvas that draws straight into an image cannot sample it, and a lit canvas can, since it draws into images of its own first.
TEST(RendererTest, RejectsDrawsThatSampleTheirOwnTarget) {
    test::EngineFixture fixture;
    graphics2d::Renderer& renderer = fixture.engine().getRenderer2D();
    const graphics::RenderTarget target = fixture.engine().getGraphics().createRenderTarget(16, 16);
    const graphics::Texture& image = target.getTexture();
    const graphics::Texture white = fixture.engine().getGraphics().getWhiteTexture();

    renderer.beginTarget(target, graphics2d::Camera{});
    EXPECT_THROW(renderer.draw({.texture = image}), std::invalid_argument);
    EXPECT_THROW(renderer.drawImageBlend({.from = white, .to = image}), std::invalid_argument);
    renderer.beginTarget(target, graphics2d::Camera{}, {.ambientLight = math::Color::white()});
    renderer.draw({.texture = image});

    renderer.beginCapture(target);
    renderer.beginScreen();
    EXPECT_THROW(renderer.draw({.texture = image}), std::invalid_argument);
    renderer.endCapture();
    renderer.beginScreen();
    renderer.draw({.texture = image});
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

// A material compiled with another version of the shader library fails when a pass first draws with it, and the pass ends with the frame, so the next frames draw normally.
TEST(RendererTest, EndsTheFrameWhenAMaterialFailsInsideAPass) {
    test::EngineFixture fixture;
    auto stale = std::make_shared<graphics::ShaderResource>();
    stale->name = "stale";
    stale->sources = {"void main() {}"};
    const core::Json program = core::Json::parse(R"({"glsl430": {"vertex": {"source": 0, "entry": "main"}, "fragment": {"source": 0, "entry": "main"}, "attrs": [], "views": [], "samplers": [], "texture_sampler_pairs": [],
        "uniform_blocks": [{"slot": 0, "stage": "vertex", "size": 64, "glsl_uniforms": [{"type": "mat4", "array_count": 1, "glsl_name": "view_projection"}]}]}})");
    for (const std::string_view name : {"sprite", "sprite_lit", "text", "text_lit", "mesh", "mesh_lit"}) {
        stale->programs[std::string(name)] = program;
    }
    const graphics2d::Material material{graphics::Shader(stale)};
    const graphics::Texture white = fixture.engine().getGraphics().getWhiteTexture();

    // clang-format off
    test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        engine.getRenderer2D().beginScreen();
        engine.getRenderer2D().draw({.texture = white, .size = {4.0F, 4.0F}, .order = {.material = material}});
    });
    // clang-format on
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string(fixture.engine().getError()->what()).find("The shader \"stale\" was compiled with another version of the shader library."), std::string::npos);

    // The error screen draws the next frames with new passes.
    fixture.frames(2);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().passes, 0U);
}

TEST(RendererTest, SortsByTheYDrawsStandOn) {
    test::EngineFixture fixture;
    const graphics::Texture first = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    const graphics::Texture second = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    const std::vector<graphics2d::SpriteInstance> column{{.position = {0.0F, 15.0F}}, {.position = {0.0F, 80.0F}}};

    // clang-format off
    const auto sorted = [&](graphics2d::Renderer::SortMode mode) {
        return test::FrameRenderer::renderOnce(fixture, [&, mode](core::Engine& engine) {
            graphics2d::Renderer& renderer = engine.getRenderer2D();
            renderer.beginWorld(graphics2d::Camera{}, {.sort = mode});
            renderer.draw({.texture = first, .position = {0.0F, 10.0F}});
            renderer.draw({.texture = second, .position = {0.0F, 50.0F}});
            renderer.draw({.texture = first, .position = {0.0F, 70.0F}, .order = {.sortOffset = -50.0F}});
            renderer.draw({.texture = second, .position = {0.0F, 60.0F}});
            renderer.drawBatch(first, column);
        }).drawCalls;
    };
    // clang-format on

    // Sorted by y, the first texture stands at 10, 15 and 20, the second one at 50 and 60, and the last sprite of the batch at 80.
    EXPECT_EQ(sorted(graphics2d::Renderer::SortMode::Y), 3U);
    EXPECT_EQ(sorted(graphics2d::Renderer::SortMode::Layer), 5U);

    // clang-format off
    const graphics2d::Renderer::Stats shapes = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(graphics2d::Camera{}, {.sort = graphics2d::Renderer::SortMode::Y});
        renderer.drawRect({0.0F, 0.0F, 10.0F, 50.0F}, math::Color::white());
        renderer.draw({.texture = second, .position = {0.0F, 20.0F}});
        renderer.drawRectOutline({0.0F, 0.0F, 10.0F, 60.0F}, 1.0F, math::Color::white());
        renderer.drawLine({0.0F, 0.0F}, {0.0F, 70.0F}, 1.0F, math::Color::white());
    });
    // clang-format on

    // Rectangles and lines stand on their lowest point, so the sprite at 20 draws first and the white quads merge after it.
    EXPECT_EQ(shapes.drawCalls, 2U);
}

TEST(RendererTest, OffsetsLayersInScopesAndMasksVisibility) {
    test::EngineFixture fixture;
    const graphics::Texture first = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    const graphics::Texture second = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        renderer.beginScreen({.visibilityMask = 0b011U});
        renderer.draw({.texture = first});
        renderer.pushLayerOffset(5);
        renderer.pushLayerOffset(-2);
        renderer.draw({.texture = second});
        renderer.popLayerOffset();
        renderer.popLayerOffset();
        renderer.draw({.texture = first});
        renderer.draw({.texture = second, .order = {.visibility = 0b100U}});
        renderer.drawRect({0.0F, 0.0F, 4.0F, 4.0F}, math::Color::white(), {.visibility = 0b100U});
        renderer.drawText(*engine.getDefaultFont(), "hidden", {}, {}, {.visibility = 0b100U});
        renderer.draw({.texture = first, .order = {.visibility = 0b010U}});
    });
    // clang-format on

    // The second texture moves three layers up, above every sprite of the first texture, and draws outside the mask never record.
    EXPECT_EQ(stats.sprites, 4U);
    EXPECT_EQ(stats.drawCalls, 2U);

    graphics2d::Renderer& renderer = fixture.engine().getRenderer2D();
    EXPECT_THROW(renderer.pushLayerOffset(1), std::logic_error);
    renderer.beginScreen();
    EXPECT_THROW(renderer.popLayerOffset(), std::logic_error);
    renderer.pushLayerOffset(1);
    renderer.beginScreen();
    EXPECT_THROW(renderer.popLayerOffset(), std::logic_error);
    fixture.frames(1);
}

TEST(RendererTest, CapturesCanvasesIntoTargets) {
    test::EngineFixture fixture;
    fixture.host().resize({640.0F, 360.0F});
    core::Engine& engine = fixture.engine();
    const graphics::Texture texture = engine.getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    const graphics::RenderTarget first = engine.getGraphics().createRenderTarget(640, 360);
    const graphics::RenderTarget second = engine.getGraphics().createRenderTarget(640, 360);
    const graphics::RenderTarget nested = engine.getGraphics().createRenderTarget(64, 64);

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& current) {
        graphics2d::Renderer& renderer = current.getRenderer2D();
        renderer.beginCapture(first, math::Color::black());
        EXPECT_TRUE(renderer.isCapturing());
        renderer.beginWorld(graphics2d::Camera{}, {.ambientLight = math::Color::black()});
        renderer.draw({.texture = texture});
        renderer.beginScreen({.order = -1});
        renderer.draw({.texture = texture});
        renderer.endCapture();

        renderer.beginCapture(second);
        renderer.beginScreen();
        renderer.draw({.texture = first.getTexture()});
        renderer.beginCapture(nested);
        renderer.beginScreen();
        renderer.draw({.texture = texture});
        renderer.endCapture();
        renderer.beginScreen();
        renderer.draw({.texture = nested.getTexture()});
        renderer.endCapture();
        EXPECT_FALSE(renderer.isCapturing());

        renderer.beginScreen();
        renderer.drawImageBlend({.from = first.getTexture(), .to = second.getTexture(), .area = renderer.getCanvasBounds(), .progress = 0.5F});
        renderer.drawImageBlend({.pattern = graphics2d::ImageBlend::Pattern::PageTurn, .from = texture, .to = second.getTexture(), .area = {0.0F, 0.0F, 10.0F, 10.0F}});
        renderer.draw({.texture = texture});
        renderer.beginCapture(second);
        renderer.beginScreen();
    });
    // clang-format on

    // The lit canvas renders its scene and light maps, then each capture, the one inside the second before it, and the capture left open, then the screen.
    EXPECT_EQ(stats.passes, 7U);
    EXPECT_EQ(stats.drawCalls, 9U);

    graphics2d::Renderer& renderer = engine.getRenderer2D();
    engine.getScenes().clear();
    EXPECT_THROW(renderer.beginCapture({}), std::invalid_argument);
    EXPECT_THROW(renderer.endCapture(), std::logic_error);
    renderer.beginCapture(first);
    renderer.beginCapture(second);
    renderer.endCapture();
    EXPECT_TRUE(renderer.isCapturing());
    renderer.beginScreen();
    EXPECT_THROW(renderer.drawImageBlend({.from = texture}), std::invalid_argument);
    renderer.endCapture();
    EXPECT_FALSE(renderer.isCapturing());
    fixture.frames(1);
    EXPECT_EQ(engine.getError(), nullptr);
}

TEST(RendererTest, DrawsCameraViewports) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    const graphics::RenderTarget target = fixture.engine().getGraphics().createRenderTarget(64, 64);
    graphics2d::Camera left;
    left.viewport = math::Rect{0.0F, 0.0F, 960.0F, 1080.0F};
    graphics2d::Camera right = left;
    right.viewport = math::Rect{960.0F, 0.0F, 960.0F, 1080.0F};
    right.setZoom({2.0F, 2.0F});
    graphics2d::Camera minimap;
    minimap.viewport = math::Rect{16.0F, 16.0F, 32.0F, 32.0F};

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(left, {.ambientLight = math::Color::black()});
        EXPECT_EQ(renderer.getCanvasBounds(), (math::Rect{-480.0F, -540.0F, 960.0F, 1080.0F}));
        renderer.draw({.texture = texture});
        renderer.beginWorld(right, {.postProcess = graphics2d::PostProcess{}});
        EXPECT_FLOAT_EQ(renderer.getCanvasUnitSize(), 0.5F);
        renderer.draw({.texture = texture});
        renderer.beginTarget(target, minimap);
        EXPECT_EQ(renderer.getCanvasBounds(), (math::Rect{-16.0F, -16.0F, 32.0F, 32.0F}));
        renderer.pushClip({-8.0F, -8.0F, 16.0F, 16.0F});
        renderer.draw({.texture = texture});
    });
    // clang-format on

    // Every lit or post-processed viewport keeps its own targets, sized to its part of the screen.
    EXPECT_EQ(stats.passes, 5U);
    graphics2d::Camera empty;
    empty.viewport = math::Rect{0.0F, 0.0F, 0.0F, 10.0F};
    fixture.engine().getRenderer2D().beginScreen();
    EXPECT_THROW(fixture.engine().getRenderer2D().beginWorld(empty), std::invalid_argument);
    EXPECT_THROW(fixture.engine().getRenderer2D().beginTarget(target, empty), std::invalid_argument);
    fixture.frames(1);
}

TEST(SpriteBatchTest, AddsUpdatesRemovesDrawsAndBakes) {
    test::EngineFixture fixture;
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
    graphics2d::SpriteBatch batch(texture);
    batch.reserve(4);
    EXPECT_EQ(batch.add({.position = {1.0F, 1.0F}, .size = {4.0F, 4.0F}}), 0U);
    EXPECT_EQ(batch.add({.position = {2.0F, 2.0F}, .size = {4.0F, 4.0F}}), 1U);
    batch.set(1, {.position = {3.0F, 3.0F}, .size = {4.0F, 4.0F}});
    EXPECT_EQ(batch.get(1).position, math::Vec2(3.0F, 3.0F));
    EXPECT_EQ(batch.getSprites().size(), 2U);
    EXPECT_EQ(batch.getTexture(), texture);

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        engine.getRenderer2D().beginScreen();
        batch.draw(engine.getRenderer2D());
    });
    // clang-format on
    EXPECT_EQ(stats.sprites, 2U);
    EXPECT_EQ(batch.bake(fixture.engine().getRenderer2D()).size(), 2U);

    batch.remove(0);
    EXPECT_EQ(batch.size(), 1U);
    EXPECT_THROW(batch.remove(5), std::out_of_range);
    EXPECT_THROW(batch.set(5, {}), std::out_of_range);
    batch.clear();
    EXPECT_EQ(batch.size(), 0U);
    EXPECT_THROW(graphics2d::SpriteBatch(graphics::Texture{}), std::invalid_argument);
}

TEST(RendererTest, LightsShadowsAndShadesLitCanvases) {
    test::EngineFixture fixture;
    graphics::Device& device = fixture.engine().getGraphics();
    const graphics::Texture texture = device.createTexture(graphics::Image(8, 8, math::Color::white()));
    const graphics::Texture normals = device.createTexture(graphics::Image(8, 8, math::Color{0.5F, 0.5F, 1.0F, 1.0F}));
    const graphics::RenderTarget target = device.createRenderTarget(64, 64);
    const graphics::RenderTarget capture = device.createRenderTarget(32, 32);
    using Light = lighting2d::Light;
    const lighting2d::Occluder wall{.points = {{10.0F, -20.0F}, {10.0F, 20.0F}, {20.0F, 20.0F}}, .cull = lighting2d::Occluder::Cull::CounterClockwise};

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(graphics2d::Camera{}, {.sort = graphics2d::Renderer::SortMode::Y, .ambientLight = math::Color{0.1F, 0.1F, 0.1F, 1.0F}});
        renderer.draw({.texture = texture, .order = {.normalMap = normals, .specular = 0.5F, .shininess = 64.0F}});
        renderer.draw({.texture = texture, .order = {.layer = 300, .emission = 2.0F, .lightMask = 2}});
        renderer.draw({.texture = texture, .order = {.blend = graphics::BlendMode::Type::Additive, .unshaded = true}});
        renderer.drawText(*engine.getDefaultFont(), "Lit", {}, {}, {.blend = graphics::BlendMode::Type::Multiply});
        renderer.drawCircle({}, 4.0F, math::Color::white(), {.blend = graphics::BlendMode::Type::Opaque});
        renderer.drawImageBlend({.from = texture, .to = texture, .area = {0.0F, 0.0F, 8.0F, 8.0F}});
        renderer.drawOccluder(wall);
        renderer.drawLight({.position = {0.0F, 0.0F}, .radius = 64.0F, .intensity = 2.5F, .height = 16.0F, .shadows = true, .shadowFilter = Light::ShadowFilter::Pcf13});
        renderer.drawLight({.type = Light::Type::Spot, .radius = 64.0F, .rotation = 1.0F, .blend = Light::Blend::Subtract, .shadows = true, .shadowFilter = Light::ShadowFilter::Pcf5});
        renderer.drawLight({.type = Light::Type::Directional, .blend = Light::Blend::Mix, .itemMask = 2, .layerMin = 1, .shadows = true});
        renderer.drawLight({.enabled = false});

        // A lit render target canvas composites into its target, and a lit canvas inside a capture into the capture.
        renderer.beginTarget(target, graphics2d::Camera{}, {.ambientLight = math::Color::black(), .postProcess = graphics2d::PostProcess{.saturation = 0.5F}});
        renderer.draw({.texture = texture});
        renderer.drawLight({.texture = texture});
        renderer.beginCapture(capture);
        renderer.beginWorld(graphics2d::Camera{}, {.ambientLight = math::Color::white()});
        renderer.draw({.texture = target.getTexture()});
        renderer.endCapture();
    });
    // clang-format on

    // The world canvas renders its scene images, its light map and the screen, the target canvas its own three passes, and the capture its canvas passes and its own.
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(stats.lights, 4U);
    EXPECT_EQ(stats.occluders, 1U);
    EXPECT_EQ(stats.shadows, 3U);
    EXPECT_EQ(stats.canvases, 3U);
    EXPECT_EQ(stats.passes, 2U + 3U + 3U + 1U);
    EXPECT_TRUE(fixture.engine().getRenderer2D().isHdrLighting());
}

TEST(RendererTest, RejectsLightingMisuse) {
    test::EngineFixture fixture;
    graphics2d::Renderer& renderer = fixture.engine().getRenderer2D();
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(graphics::Image(2, 2, math::Color::white()));
    const lighting2d::Occluder wall{.points = {{0.0F, 0.0F}, {5.0F, 0.0F}}, .closed = false};

    renderer.beginWorld(graphics2d::Camera{});
    EXPECT_THROW(renderer.drawLight({}), std::logic_error);
    EXPECT_THROW(renderer.drawOccluder(wall), std::logic_error);
    renderer.beginTarget(fixture.engine().getGraphics().createRenderTarget(8, 8), graphics2d::Camera{}, {.ambientLight = math::Color::black()});
    renderer.drawOccluder(wall);
    EXPECT_THROW(renderer.drawOccluder({.points = {{0.0F, 0.0F}}}), std::invalid_argument);
    EXPECT_THROW(renderer.drawLight({.radius = -1.0F}), std::invalid_argument);
    EXPECT_THROW(renderer.draw({.texture = texture, .order = {.shininess = 0.0F}}), std::invalid_argument);
    EXPECT_THROW(renderer.draw({.texture = texture, .order = {.emission = -1.0F}}), std::invalid_argument);
    fixture.frames(1);
}

TEST(RendererTest, DrawsMetaballsFromManyPoints) {
    test::EngineFixture fixture;
    std::vector<math::Vec2> points;
    for (int index = 0; index < 2000; ++index) {
        points.push_back(math::Vec2::fromAngle(static_cast<float>(index) * 2.4F, static_cast<float>(index % 40)));
    }

    // clang-format off
    const graphics2d::Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(graphics2d::Camera{});
        renderer.drawMetaballs(points, 6.0F, {.color = math::Color{0.2F, 0.5F, 1.0F, 1.0F}, .outlineColor = math::Color::white(), .outlineWidth = 0.1F});
        renderer.drawMetaballs(std::span<const math::Vec2>{}, 6.0F);
        renderer.beginWorld(graphics2d::Camera{}, {.ambientLight = math::Color::white()});
        renderer.drawMetaballs(points, 3.0F, {.threshold = 0.3F}, {.layer = 2});
        renderer.beginScreen({.visibilityMask = 2});
        renderer.drawMetaballs(points, 3.0F);
    });
    // clang-format on

    // Each drawn surface fills its own field in one instanced pass, and a draw the visibility mask hides fills nothing.
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(stats.instances, 0U);
    EXPECT_EQ(stats.passes, 2U + 2U + 1U);
    EXPECT_EQ(stats.drawCalls, 2U + 2U + 1U);

    graphics2d::Renderer& renderer = fixture.engine().getRenderer2D();
    renderer.beginScreen();
    EXPECT_THROW(renderer.drawMetaballs(points, 0.0F), std::invalid_argument);
    EXPECT_THROW(renderer.drawMetaballs(points, 4.0F, {.threshold = 1.0F}), std::invalid_argument);
    EXPECT_THROW(renderer.drawMetaballs(points, 4.0F, {.outlineWidth = -0.1F}), std::invalid_argument);
    EXPECT_THROW(renderer.drawMetaballs(points, 4.0F, {.outlineWidth = 0.6F}), std::invalid_argument);
    fixture.frames(1);
}

} // namespace haylen
