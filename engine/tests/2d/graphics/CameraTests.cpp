#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Parallax.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Math.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

namespace {

constexpr math::Rect kScreen{0.0F, 0.0F, 200.0F, 100.0F};

graphics2d::Renderer::Stats renderOnce(test::EngineFixture& fixture, std::function<void(core::Engine&)> draw) {
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>(std::move(draw)));
    fixture.frames(1);
    return fixture.engine().getRenderer2D().getStats();
}

} // namespace

TEST(CameraTest, FollowsThroughTheDeadZoneSmoothingAndLimits) {
    graphics2d::Camera camera;
    camera.limits = math::Rect{0.0F, 0.0F, 1000.0F, 1000.0F};
    camera.deadZone = {20.0F, 20.0F};
    camera.follow({5.0F, 5.0F}, 1.0F, kScreen);
    EXPECT_EQ(camera.position, math::Vec2(100.0F, 50.0F));

    // The target moves ten units each way from the goal before the view follows it.
    camera.snapTo({500.0F, 500.0F}, kScreen);
    camera.follow({505.0F, 500.0F}, 1.0F, kScreen);
    EXPECT_EQ(camera.position, math::Vec2(500.0F, 500.0F));
    camera.follow({600.0F, 500.0F}, 1.0F, kScreen);
    EXPECT_FLOAT_EQ(camera.position.x, 590.0F);

    camera.positionSmoothing = true;
    camera.follow({700.0F, 500.0F}, 0.1F, kScreen);
    EXPECT_NEAR(camera.position.x, 590.0F + 100.0F * (1.0F - std::exp(-0.5F)), 1e-3F);
    camera.resetSmoothing(kScreen);
    EXPECT_FLOAT_EQ(camera.position.x, 690.0F);

    // A limit smaller than the view centers the view on it.
    camera.limits = math::Rect{0.0F, 0.0F, 100.0F, 50.0F};
    camera.clampToLimits(kScreen);
    EXPECT_EQ(camera.position, math::Vec2(50.0F, 25.0F));

    // With limit smoothing the view eases toward the limit instead of stopping at it.
    graphics2d::Camera eased;
    eased.limits = math::Rect{0.0F, 0.0F, 1000.0F, 1000.0F};
    eased.positionSmoothing = true;
    eased.limitSmoothing = true;
    eased.snapTo({500.0F, 500.0F}, kScreen);
    eased.follow({-400.0F, 500.0F}, 0.1F, kScreen);
    EXPECT_NEAR(eased.position.x, 500.0F - 400.0F * (1.0F - std::exp(-0.5F)), 1e-3F);
}

TEST(CameraTest, DragsInsideItsMarginsAndRestsAtTheDragOffset) {
    graphics2d::Camera camera;
    camera.dragHorizontal = true;
    camera.dragMargins = {0.5F, 0.2F, 0.25F, 0.2F};
    camera.snapTo({}, kScreen);

    // Half the view is 100 units wide, so the target moves 50 units left and 25 units right freely.
    camera.follow({-40.0F, 10.0F}, 1.0F, kScreen);
    EXPECT_EQ(camera.position, math::Vec2(0.0F, 10.0F));
    camera.follow({-60.0F, 10.0F}, 1.0F, kScreen);
    EXPECT_FLOAT_EQ(camera.position.x, -10.0F);
    camera.follow({30.0F, 10.0F}, 1.0F, kScreen);
    EXPECT_FLOAT_EQ(camera.position.x, 5.0F);

    // The vertical axis does not drag, so the target rests on the margin the offset points at.
    camera.dragOffset = {0.0F, -1.0F};
    camera.follow({30.0F, 10.0F}, 1.0F, kScreen);
    EXPECT_FLOAT_EQ(camera.position.y, 0.0F);
    camera.dragOffset = {0.0F, 1.0F};
    camera.follow({30.0F, 10.0F}, 1.0F, kScreen);
    EXPECT_FLOAT_EQ(camera.position.y, 20.0F);

    camera.align(kScreen);
    EXPECT_FLOAT_EQ(camera.position.x, 5.0F);
    camera.follow({30.0F, 10.0F}, 1.0F, kScreen);
    EXPECT_FLOAT_EQ(camera.position.x, 30.0F);

    graphics2d::Camera still;
    still.position = {7.0F, 3.0F};
    still.align(kScreen);
    still.resetSmoothing(kScreen);
    EXPECT_EQ(still.position, math::Vec2(7.0F, 3.0F));
}

TEST(CameraTest, LooksAheadOfAMovingTarget) {
    graphics2d::Camera camera;
    camera.lookAheadTime = 0.5F;
    camera.lookAheadSmoothingSpeed = 1000.0F;
    camera.maxLookAhead = 1000.0F;
    camera.snapTo({}, kScreen);
    for (int step = 1; step <= 10; ++step) {
        camera.follow({static_cast<float>(step) * 10.0F, 0.0F}, 0.1F, kScreen);
    }

    // The target moves at 100 units per second, so the view leads it by 50 units.
    EXPECT_NEAR(camera.position.x, 150.0F, 1e-2F);
    camera.maxLookAhead = 20.0F;
    camera.follow({110.0F, 0.0F}, 0.1F, kScreen);
    EXPECT_NEAR(camera.position.x, 130.0F, 1e-2F);
    camera.follow({110.0F, 0.0F}, 0.0F, kScreen);
    EXPECT_NEAR(camera.position.x, 130.0F, 1e-2F);
}

TEST(CameraTest, ZoomsWithinLimitsAroundPointsAndFramesTargets) {
    graphics2d::Camera camera;
    camera.setZoom({2.0F, 30.0F});
    EXPECT_EQ(camera.getZoom(), math::Vec2(2.0F, 20.0F));
    camera.setMaxZoom(4.0F);
    EXPECT_EQ(camera.getZoom(), math::Vec2(2.0F, 4.0F));
    EXPECT_THROW(camera.setMinZoom(0.0F), std::invalid_argument);
    EXPECT_THROW(camera.setMinZoom(5.0F), std::invalid_argument);
    EXPECT_THROW(camera.setMaxZoom(0.01F), std::invalid_argument);
    camera.setMinZoom(0.5F);
    EXPECT_FLOAT_EQ(camera.getMinZoom(), 0.5F);
    EXPECT_FLOAT_EQ(camera.getMaxZoom(), 4.0F);

    // Zooming at a screen point keeps the world point under it in place.
    camera.setZoom({1.0F, 1.0F});
    const math::Vec2 point{150.0F, 25.0F};
    const math::Vec2 before = camera.screenToWorld(point, kScreen);
    camera.zoomAt(2.0F, point, kScreen);
    const math::Vec2 after = camera.screenToWorld(point, kScreen);
    EXPECT_EQ(camera.getZoom(), math::Vec2(2.0F, 2.0F));
    EXPECT_NEAR(after.x, before.x, 1e-3F);
    EXPECT_NEAR(after.y, before.y, 1e-3F);

    // Padded by 10 units the points span 120 by 60 units, which fit the 200 by 100 view at a zoom of 5/3.
    graphics2d::Camera framing;
    const std::array<math::Vec2, 3> points{math::Vec2{0.0F, 0.0F}, math::Vec2{100.0F, 0.0F}, math::Vec2{50.0F, 40.0F}};
    framing.frame(points, 10.0F, 1.0F, kScreen);
    EXPECT_NEAR(framing.getZoom().x, 5.0F / 3.0F, 1e-4F);
    EXPECT_EQ(framing.position, math::Vec2(50.0F, 20.0F));
    framing.setMaxZoom(1.0F);
    framing.frame(std::array<math::Vec2, 1>{math::Vec2{5.0F, 5.0F}}, 0.0F, 1.0F, kScreen);
    EXPECT_EQ(framing.getZoom(), math::Vec2(1.0F, 1.0F));
    framing.positionSmoothing = true;
    framing.setZoom({0.5F, 0.5F});
    framing.frame(points, 10.0F, 0.1F, kScreen);
    EXPECT_GT(framing.getZoom().x, 0.5F);
    EXPECT_LT(framing.getZoom().x, 1.0F);
    EXPECT_THROW(framing.frame({}, 0.0F, 1.0F, kScreen), std::invalid_argument);

    graphics2d::Camera topLeft;
    topLeft.anchor = graphics2d::Camera::Anchor::TopLeft;
    topLeft.frame(points, 10.0F, 1.0F, kScreen);
    EXPECT_NEAR(topLeft.worldToScreen({50.0F, 20.0F}, kScreen).x, 100.0F, 1e-3F);
}

TEST(CameraTest, AnchorsRotatesAndConvertsThroughViewports) {
    graphics2d::Camera camera;
    camera.setZoom({2.0F, 2.0F});
    EXPECT_EQ(camera.worldToScreen({10.0F, 0.0F}, kScreen), math::Vec2(120.0F, 50.0F));
    EXPECT_EQ(camera.viewTransform(kScreen.getSize()).apply({10.0F, 0.0F}), math::Vec2(120.0F, 50.0F));
    EXPECT_NEAR(camera.screenToWorld({120.0F, 50.0F}, kScreen).x, 10.0F, 1e-4F);
    EXPECT_EQ(camera.visibleBounds(kScreen), (math::Rect{-50.0F, -25.0F, 100.0F, 50.0F}));

    camera.viewport = math::Rect{100.0F, 0.0F, 100.0F, 100.0F};
    EXPECT_EQ(camera.worldToScreen({}, kScreen), math::Vec2(150.0F, 50.0F));
    EXPECT_EQ(camera.visibleBounds(kScreen), (math::Rect{-25.0F, -25.0F, 50.0F, 50.0F}));
    EXPECT_EQ(camera.getViewRect(kScreen), (math::Rect{100.0F, 0.0F, 100.0F, 100.0F}));

    camera.viewport.reset();
    camera.anchor = graphics2d::Camera::Anchor::TopLeft;
    EXPECT_EQ(camera.worldToScreen({}, kScreen), math::Vec2(0.0F, 0.0F));
    EXPECT_EQ(camera.worldToScreen({10.0F, 5.0F}, kScreen), math::Vec2(20.0F, 10.0F));

    graphics2d::Camera turned;
    turned.rotation = math::Math::kHalfPi;
    const math::Vec2 rotated = turned.worldToScreen({10.0F, 0.0F}, kScreen);
    EXPECT_NEAR(rotated.x, 100.0F, 1e-4F);
    EXPECT_NEAR(rotated.y, 40.0F, 1e-4F);
    turned.ignoreRotation = true;
    EXPECT_EQ(turned.worldToScreen({10.0F, 0.0F}, kScreen), math::Vec2(110.0F, 50.0F));

    graphics2d::Camera smooth;
    smooth.rotationSmoothing = true;
    smooth.rotation = 1.0F;
    smooth.update(0.1F);
    EXPECT_NEAR(smooth.getRenderRotation(), 1.0F - std::exp(-0.5F), 1e-4F);
    smooth.resetSmoothing(kScreen);
    EXPECT_FLOAT_EQ(smooth.getRenderRotation(), 1.0F);
}

TEST(CameraTest, ShakesWithTraumaFrequencyAndDirection) {
    graphics2d::Camera camera;
    camera.addTrauma(0.8F);
    camera.addTrauma(0.8F);
    EXPECT_FLOAT_EQ(camera.getTrauma(), 1.0F);
    camera.update(0.1F);
    EXPECT_LT(camera.getTrauma(), 1.0F);
    EXPECT_FALSE(camera.getShakeOffset().isZero());
    EXPECT_NE(camera.getRenderRotation(), 0.0F);
    camera.setTrauma(2.0F);
    EXPECT_FLOAT_EQ(camera.getTrauma(), 1.0F);

    // A directional shake moves only along its direction and never turns the view.
    camera.setTrauma(0.0F);
    camera.shake(1.0F, {0.0F, 3.0F});
    camera.update(0.05F);
    EXPECT_FLOAT_EQ(camera.getShakeOffset().x, 0.0F);
    EXPECT_NE(camera.getShakeOffset().y, 0.0F);
    EXPECT_FLOAT_EQ(camera.getRenderRotation(), 0.0F);

    // At a frequency of zero the noise stands still, so only the fading trauma changes the offset.
    graphics2d::Camera frozen;
    frozen.shakeFrequency = 0.0F;
    frozen.addTrauma(1.0F);
    frozen.update(0.1F);
    const math::Vec2 first = frozen.getShakeOffset();
    frozen.update(0.1F);
    const math::Vec2 second = frozen.getShakeOffset();
    EXPECT_NEAR(second.x * 0.85F * 0.85F, first.x * 0.7F * 0.7F, 1e-4F);

    camera.pixelSnap = true;
    camera.setZoom({2.0F, 2.0F});
    camera.addTrauma(0.5F);
    camera.update(0.02F);
    const math::Vec2 snapped = camera.getRenderPosition() * camera.getZoom();
    EXPECT_FLOAT_EQ(snapped.x, std::round(snapped.x));
    camera.update(10.0F);
    EXPECT_FLOAT_EQ(camera.getTrauma(), 0.0F);
    EXPECT_TRUE(camera.getShakeOffset().isZero());
}

TEST(CameraTest, BlendsBetweenTwoViews) {
    graphics2d::Camera from;
    from.viewport = math::Rect{100.0F, 100.0F, 300.0F, 300.0F};
    graphics2d::Camera to;
    to.position = {100.0F, 50.0F};
    to.setZoom({4.0F, 4.0F});
    to.rotation = 1.0F;
    to.viewport = math::Rect{0.0F, 0.0F, 100.0F, 100.0F};

    const graphics2d::Camera half = graphics2d::Camera::blend(from, to, 0.5F);
    EXPECT_EQ(half.position, math::Vec2(50.0F, 25.0F));
    EXPECT_NEAR(half.getZoom().x, 2.0F, 1e-4F);
    EXPECT_FLOAT_EQ(half.getRenderRotation(), 0.5F);
    EXPECT_EQ(half.viewport, (math::Rect{50.0F, 50.0F, 200.0F, 200.0F}));
    from.viewport.reset();
    EXPECT_EQ(graphics2d::Camera::blend(from, to, 0.25F).viewport, to.viewport);
}

TEST(CameraTest, DrawsItsDebugShapes) {
    test::EngineFixture fixture;
    const math::Rect screen = fixture.engine().getViewport().getVisibleRect();
    graphics2d::Camera camera;
    camera.limits = math::Rect{-2000.0F, -2000.0F, 4000.0F, 4000.0F};
    camera.deadZone = {40.0F, 40.0F};
    camera.follow({10.0F, 10.0F}, 0.1F, screen);

    // clang-format off
    const auto sprites = [&] {
        return renderOnce(fixture, [&](core::Engine& engine) {
            engine.getRenderer2D().beginWorld(camera);
            camera.drawDebug(engine.getRenderer2D(), screen);
        }).sprites;
    };
    // clang-format on

    // The view and the limits have four edges each, the box of the target four more, and its cross two lines.
    EXPECT_EQ(sprites(), 14U);
    camera.anchor = graphics2d::Camera::Anchor::TopLeft;
    EXPECT_EQ(sprites(), 8U);
}

TEST(ParallaxTest, ScrollsRepeatsAndStopsOutsideItsLimits) {
    test::EngineFixture fixture;
    const math::Rect screen = fixture.engine().getViewport().getVisibleRect();
    graphics2d::Parallax layer;
    layer.texture = fixture.engine().getGraphics().createTexture(graphics::Image(100, 50, math::Color::white()));
    layer.scrollScale = {0.5F, 1.0F};
    graphics2d::Camera camera;
    camera.position = {200.0F, 0.0F};
    EXPECT_EQ(layer.getOffset(camera), math::Vec2(100.0F, 0.0F));

    layer.autoscroll = {10.0F, 0.0F};
    layer.update(2.0F);
    EXPECT_EQ(layer.getScrolled(), math::Vec2(20.0F, 0.0F));
    EXPECT_EQ(layer.getOffset(camera), math::Vec2(120.0F, 0.0F));
    layer.limits = math::Rect{0.0F, 0.0F, 100.0F, 100.0F};
    EXPECT_EQ(layer.getOffset(camera), math::Vec2(70.0F, 0.0F));

    // clang-format off
    const auto sprites = [&] {
        return renderOnce(fixture, [&](core::Engine& engine) {
            engine.getRenderer2D().beginWorld(camera);
            layer.draw(engine.getRenderer2D(), camera, screen, {.layer = -1});
        }).sprites;
    };
    // clang-format on

    // The view spans x from -760 to 1160, which copies 100 units apart cover in 20 steps, and 1080 units of y take 22 rows of 50.
    EXPECT_EQ(sprites(), 1U);
    layer.repeatX = true;
    EXPECT_EQ(sprites(), 20U);
    layer.repeatY = true;
    EXPECT_EQ(sprites(), 440U);
    layer.repeatSize = {200.0F, 100.0F};
    layer.size = {150.0F, 90.0F};
    layer.source = math::Rect{0.0F, 0.0F, 50.0F, 50.0F};
    EXPECT_EQ(sprites(), 132U);

    graphics2d::Renderer& renderer = fixture.engine().getRenderer2D();
    layer.repeatSize = {};
    layer.size = {0.0F, 10.0F};
    fixture.engine().getScenes().clear();
    renderer.beginWorld(camera);
    EXPECT_THROW(layer.draw(renderer, camera, screen), std::invalid_argument);
    layer.texture = {};
    EXPECT_THROW(layer.draw(renderer, camera, screen), std::logic_error);
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen
