#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/CameraRig.hpp"
#include "haylen/2d/graphics/PixelCanvas.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

namespace {

class CameraRigTest : public ::testing::Test {
  protected:
    const math::Rect screen{0.0F, 0.0F, 200.0F, 100.0F};
};

} // namespace

TEST_F(CameraRigTest, HoldsTheCameraInTheZoneOfTheTargetAndEasesBetweenZones) {
    CameraRig rig({.limits = math::Rect{0.0F, 0.0F, 2000.0F, 1000.0F}, .blendTime = 1.0F});
    rig.addZone({.name = "hall", .area = {0.0F, 0.0F, 400.0F, 300.0F}});
    rig.addZone({.name = "arena", .area = {400.0F, 0.0F, 600.0F, 300.0F}, .limits = math::Rect{400.0F, 0.0F, 600.0F, 400.0F}, .zoom = math::Vec2{0.5F, 0.5F}, .offset = {0.0F, -20.0F}});
    rig.addZone({.name = "boss", .area = {700.0F, 0.0F, 100.0F, 100.0F}, .priority = 2});
    Camera camera;

    rig.update(camera, {100.0F, 100.0F}, 0.0F, screen);
    ASSERT_NE(rig.getZone(), nullptr);
    EXPECT_EQ(rig.getZone()->name, "hall");
    EXPECT_EQ(camera.limits, (math::Rect{0.0F, 0.0F, 400.0F, 300.0F}));

    // Entering another zone eases the limits, the zoom and the offset over the blend time.
    rig.update(camera, {500.0F, 150.0F}, 0.5F, screen);
    EXPECT_EQ(rig.getZone()->name, "arena");
    EXPECT_NEAR(camera.getZoom().x, 0.75F, 0.001F);
    EXPECT_NEAR(camera.offset.y, -10.0F, 0.001F);
    EXPECT_NEAR(camera.limits->getBottom(), 350.0F, 0.001F);
    rig.update(camera, {500.0F, 150.0F}, 0.6F, screen);
    EXPECT_EQ(camera.getZoom(), math::Vec2(0.5F, 0.5F));
    EXPECT_EQ(camera.limits, (math::Rect{400.0F, 0.0F, 600.0F, 400.0F}));

    // A zone of higher priority wins where zones overlap, and outside every zone the camera takes the limits of the rig.
    rig.update(camera, {750.0F, 50.0F}, 2.0F, screen);
    EXPECT_EQ(rig.getZone()->name, "boss");
    rig.update(camera, {1500.0F, 500.0F}, 2.0F, screen);
    EXPECT_EQ(rig.getZone(), nullptr);
    EXPECT_EQ(camera.limits, (math::Rect{0.0F, 0.0F, 2000.0F, 1000.0F}));
    EXPECT_EQ(camera.getZoom(), math::Vec2(1.0F, 1.0F));

    rig.removeZone("boss");
    EXPECT_EQ(rig.getZones().size(), 2U);
    rig.addZone({.name = "hall", .area = {0.0F, 0.0F, 10.0F, 10.0F}});
    EXPECT_EQ(rig.getZones().size(), 2U);
    EXPECT_THROW(rig.addZone({.name = "", .area = {0.0F, 0.0F, 1.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(rig.addZone({.name = "flat", .area = {0.0F, 0.0F, 0.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(rig.addZone({.name = "inverted", .area = {0.0F, 0.0F, 1.0F, 1.0F}, .zoom = math::Vec2{0.0F, 1.0F}}), std::invalid_argument);
    EXPECT_THROW(CameraRig({.blendTime = -1.0F}), std::invalid_argument);
}

TEST_F(CameraRigTest, FollowsTheTargetAlongTheRail) {
    CameraRig rig({});
    rig.setRail({{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}});
    EXPECT_EQ(rig.project({50.0F, 30.0F}), math::Vec2(50.0F, 0.0F));
    EXPECT_NEAR(rig.project({150.0F, 60.0F}).x, 100.0F, 1e-4F);
    EXPECT_NEAR(rig.project({150.0F, 60.0F}).y, 60.0F, 1e-4F);
    EXPECT_EQ(rig.project({-50.0F, -5.0F}), math::Vec2(0.0F, 0.0F));
    Camera camera;
    rig.update(camera, {40.0F, 20.0F}, 1.0F / 60.0F, screen);
    EXPECT_EQ(camera.position, math::Vec2(40.0F, 0.0F));
    EXPECT_THROW(rig.setRail({{1.0F, 1.0F}}), std::invalid_argument);
    rig.setRail({});
    EXPECT_EQ(rig.project({5.0F, 6.0F}), math::Vec2(5.0F, 6.0F));
}

TEST(PixelCanvasTest, RendersAtWholePixelsAndShiftsByThePartBetween) {
    test::EngineFixture fixture;
    PixelCanvas canvas(fixture.engine().getGraphics(), 320, 180, {});
    EXPECT_EQ(canvas.getTarget().getSize(), math::Vec2(322.0F, 182.0F));
    Camera camera;
    camera.position = {10.25F, -3.75F};
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        canvas.begin(engine.getRenderer2D(), camera, {});
        engine.getRenderer2D().drawRect({0.0F, 0.0F, 4.0F, 4.0F}, math::Color::white());
        engine.getRenderer2D().beginScreen();
        canvas.draw(engine.getRenderer2D(), {0.0F, 0.0F, 1280.0F, 720.0F});
    });
    // clang-format on
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(stats.canvases, 2U);
    EXPECT_NEAR(canvas.getFraction().x, 0.25F, 1e-5F);
    EXPECT_NEAR(canvas.getFraction().y, 0.25F, 1e-5F);

    PixelCanvas blocky(fixture.engine().getGraphics(), 64, 64, {.smooth = false});
    test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) { blocky.begin(engine.getRenderer2D(), camera, {}); });
    EXPECT_EQ(blocky.getFraction(), math::Vec2(0.0F, 0.0F));
    EXPECT_THROW(PixelCanvas(fixture.engine().getGraphics(), 0, 10, {}), std::invalid_argument);
}

} // namespace haylen::graphics2d
