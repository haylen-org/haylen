#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Glass.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

class GlassTest : public ::testing::Test {
  protected:
    // Runs the call and records the message of what it throws, or an empty message when it throws nothing.
    static void record(std::vector<std::string>& errors, const std::function<void()>& call) {
        try {
            call();
            errors.emplace_back();
        } catch (const std::exception& error) {
            errors.emplace_back(error.what());
        }
    }

    // A camera that shows a render target with the world origin at its top-left corner, one unit to a pixel.
    [[nodiscard]] static Camera topLeft() {
        Camera camera;
        camera.anchor = Camera::Anchor::TopLeft;
        return camera;
    }

    test::EngineFixture fixture;
    Camera camera;
};

// The screen draws into an image of its own while its canvases hold glass, so the glass reads what every canvas before it drew. A blur of 16 pixels shrinks to half the size and to a quarter, where it blurs across and down, grows back and reaches the image of the glass.
TEST_F(GlassTest, ReadsTheScreenThroughAnImageOfItsOwn) {
    // clang-format off
    const Renderer::Stats plain = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(camera);
        renderer.drawRect({-200.0F, -200.0F, 400.0F, 400.0F}, math::Color::white());
    });
    const Renderer::Stats glassy = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(camera);
        renderer.drawRect({-200.0F, -200.0F, 400.0F, 400.0F}, math::Color::white());
        renderer.beginScreen();
        renderer.drawGlass({.bounds = {100.0F, 100.0F, 400.0F, 120.0F}, .radii = {24.0F, 24.0F, 24.0F, 24.0F}}, {.blur = 16.0F, .rim = 0.5F, .refraction = 8.0F});
    });
    const Renderer::Stats sharp = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginScreen();
        renderer.drawGlass({.bounds = {100.0F, 100.0F, 400.0F, 120.0F}}, {.blur = 0.0F});
    });
    // clang-format on
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(plain.passes, 1U);
    EXPECT_EQ(glassy.passes, 9U);

    // A glass without a blur copies what lies behind it in one pass.
    EXPECT_EQ(sharp.passes, 4U);
}

// A render target canvas holds what its glass reads, so the screen keeps drawing into the swapchain. A blur of 8 pixels spans 4 texels at half the size, where it blurs at once.
TEST_F(GlassTest, ReadsARenderTargetCanvasWhereItDraws) {
    const graphics::RenderTarget target = fixture.engine().getGraphics().createRenderTarget(256, 256);
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginTarget(target, topLeft(), {.clear = math::Color::black()});
        renderer.drawCircle({128.0F, 128.0F}, 60.0F, math::Color::white());
        renderer.drawGlass({.bounds = {32.0F, 96.0F, 192.0F, 64.0F}}, {.blur = 8.0F});
        renderer.drawCircle({128.0F, 128.0F}, 10.0F, math::Color::white(), {.layer = 1});
    });
    // clang-format on
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(stats.passes, 7U);
    EXPECT_EQ(stats.drawCalls, 7U);
}

// A glass that its clip hides reads nothing and keeps the pass of its canvas.
TEST_F(GlassTest, SkipsAGlassOutsideItsClip) {
    const graphics::RenderTarget target = fixture.engine().getGraphics().createRenderTarget(256, 256);
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginTarget(target, topLeft());
        renderer.pushClip({0.0F, 0.0F, 64.0F, 64.0F});
        renderer.drawGlass({.bounds = {128.0F, 128.0F, 64.0F, 64.0F}}, {});
        renderer.popClip();
    });
    // clang-format on
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(stats.passes, 2U);
}

TEST_F(GlassTest, RejectsWhatAGlassCannotDraw) {
    std::vector<std::string> errors;
    const graphics::Texture white = fixture.engine().getGraphics().getWhiteTexture();
    // clang-format off
    test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginScreen();
        record(errors, [&renderer, &white] { renderer.drawGlass({.bounds = {0.0F, 0.0F, 8.0F, 8.0F}, .texture = white}, {}); });
        record(errors, [&renderer] { renderer.drawGlass({.bounds = {0.0F, 0.0F, 8.0F, 8.0F}, .softness = -1.0F}, {}); });
        record(errors, [&renderer] { renderer.drawGlass({.bounds = {0.0F, 0.0F, 8.0F, 8.0F}}, {.blur = -1.0F}); });
        record(errors, [&renderer] { renderer.drawGlass({.bounds = {0.0F, 0.0F, 8.0F, 8.0F}}, {.rim = 1.5F}); });
        renderer.beginWorld(camera, {.ambientLight = math::Color(0.5F, 0.5F, 0.5F, 1.0F)});
        record(errors, [&renderer] { renderer.drawGlass({.bounds = {0.0F, 0.0F, 8.0F, 8.0F}}, {}); });
    });
    // clang-format on
    ASSERT_EQ(errors.size(), 5U);
    EXPECT_EQ(errors[0], "A glass shows the image behind it, so its shape takes no texture.");
    EXPECT_EQ(errors[1], "A glass needs corner radii, a border width and a softness of zero or more.");
    EXPECT_EQ(errors[2], "A glass needs a blur, a saturation, a brightness, a rim width and a refraction of zero or more.");
    EXPECT_EQ(errors[3], "A glass needs a rim from 0 to 1.");
    EXPECT_EQ(errors[4], "A glass draws in a canvas without ambient light. Draw it in a canvas that begins after the lit one.");
}

TEST_F(GlassTest, DrawsTheSameGlassEveryFrameWithoutAllocating) {
    const graphics::RenderTarget target = fixture.engine().getGraphics().createRenderTarget(256, 256);
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginTarget(target, topLeft(), {.clear = math::Color::black()});
        renderer.drawRect({0.0F, 0.0F, 128.0F, 256.0F}, math::Color::white());
        renderer.drawGlass({.bounds = {64.0F, 64.0F, 128.0F, 128.0F}, .rotation = 0.3F}, {.blur = 40.0F, .refraction = 6.0F});
        renderer.beginWorld(camera);
        renderer.drawRect({-200.0F, -200.0F, 400.0F, 400.0F}, math::Color::white());
        renderer.beginScreen();
        renderer.pushFade({0.0F, 0.0F, 600.0F, 300.0F}, {.length = 40.0F, .bottom = 1.0F});
        renderer.drawGlass({.kind = Shape::Kind::Ellipse, .bounds = {100.0F, 100.0F, 400.0F, 120.0F}}, {.blur = 24.0F, .tint = math::Color(1.0F, 1.0F, 1.0F, 0.3F)});
        renderer.drawGlass({.bounds = {100.0F, 300.0F, 400.0F, 120.0F}}, {.blur = 0.0F});
        renderer.popFade();
    }));
    // clang-format on
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    const test::AllocationTracker tracker;
    fixture.frames(5);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

} // namespace haylen::graphics2d
