#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/text/Font.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

namespace {

class GroupTest : public ::testing::Test {
  protected:
    test::EngineFixture fixture;
    Camera camera;
};

} // namespace

TEST_F(GroupTest, RendersGroupsApartAndDrawsThemAsOneImage) {
    std::size_t visited = 0;
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        text::Font& font = *engine.getDefaultFont();
        renderer.beginWorld(camera);
        renderer.drawText(font, "Before", {0.0F, 0.0F});
        renderer.pushClip({-50.0F, -50.0F, 100.0F, 100.0F});
        renderer.beginGroup({.opacity = 0.5F, .order = {.layer = 2}});
        renderer.drawText(font, "Inside", {0.0F, 20.0F});
        renderer.drawText(font, "Inside", {0.0F, 40.0F});
        renderer.endGroup();
        renderer.popClip();
        renderer.drawText(font, "After", {0.0F, 60.0F});
        visited = 0;
        renderer.visitDrawn([&visited](const Renderer::Drawn&) { ++visited; });
    });
    // clang-format on
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    // The canvas keeps only its own draws around the image of the group, which renders in a pass of its own.
    EXPECT_EQ(visited, 2U);
    EXPECT_EQ(stats.canvases, 2U);
    EXPECT_EQ(stats.passes, 2U);
}

TEST_F(GroupTest, NestsGroupsAndMasksWithEveryKindOfDraw) {
    const std::array<math::Vec2, 3> points{{{0.0F, 0.0F}, {8.0F, 0.0F}, {4.0F, 6.0F}}};
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(camera);
        renderer.drawMetaballs(points, 4.0F);
        renderer.pushMask(Renderer::MaskMode::Outside, {.layer = 1});
        renderer.drawCircle({0.0F, 0.0F}, 30.0F, math::Color::white());
        renderer.endMaskShape();
        renderer.beginGroup({.opacity = 0.25F});
        renderer.drawMetaballs(points, 6.0F);
        renderer.drawStroke(points, {.width = 3.0F});
        renderer.endGroup();
        renderer.drawRect({0.0F, 0.0F, 50.0F, 50.0F}, math::Color::white());
        renderer.popMask();
        renderer.drawMetaballs(points, 2.0F);
        renderer.beginScreen();
        renderer.drawRect({0.0F, 0.0F, 5.0F, 5.0F}, math::Color::white());
    });
    // clang-format on
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    // The world canvas, the shape and the content of the mask, the group inside the content and the screen canvas.
    EXPECT_EQ(stats.canvases, 5U);
}

TEST_F(GroupTest, ClosesWhatACanvasLeavesOpen) {
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(camera);
        renderer.beginGroup({});
        renderer.pushMask();
        renderer.drawRect({0.0F, 0.0F, 5.0F, 5.0F}, math::Color::white());
        renderer.beginScreen();
        renderer.beginGroup({});
        renderer.pushMask();
        renderer.endMaskShape();
    });
    // clang-format on
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(stats.canvases, 8U);
}

TEST_F(GroupTest, RejectsCallsThatMatchNothing) {
    std::vector<std::string> errors;
    // clang-format off
    test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        const auto expect = [&errors](const std::function<void()>& call) {
            try {
                call();
                errors.emplace_back();
            } catch (const std::exception& error) {
                errors.emplace_back(error.what());
            }
        };
        expect([&renderer] { renderer.beginGroup({}); });
        renderer.beginWorld(camera);
        expect([&renderer] { renderer.endGroup(); });
        expect([&renderer] { renderer.popMask(); });
        expect([&renderer] { renderer.endMaskShape(); });
        expect([&renderer] { renderer.beginGroup({.opacity = 2.0F}); });
        renderer.pushMask();
        expect([&renderer] { renderer.popMask(); });
        expect([&renderer] { renderer.endGroup(); });
    });
    // clang-format on
    ASSERT_EQ(errors.size(), 7U);
    EXPECT_NE(errors[0].find("No canvas is active."), std::string::npos);
    EXPECT_EQ(errors[1], "The \"endGroup\" call has no matching \"beginGroup\".");
    EXPECT_EQ(errors[2], "The \"popMask\" call has no matching \"pushMask\" whose shape ended with \"endMaskShape\".");
    EXPECT_EQ(errors[3], "The \"endMaskShape\" call has no matching \"pushMask\".");
    EXPECT_EQ(errors[4], "A group needs an opacity from 0 to 1.");
    EXPECT_EQ(errors[5], errors[2]);
    EXPECT_EQ(errors[6], errors[1]);
}

TEST_F(GroupTest, BlendsAGroupWithACopyOfWhatLiesBelowIt) {
    // clang-format off
    const Renderer::Stats stats = test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(camera);
        renderer.drawRect({0.0F, 0.0F, 80.0F, 80.0F}, math::Color::white());
        renderer.beginGroup({.order = {.layer = 1, .blend = graphics::BlendMode::Type::Overlay}});
        renderer.drawRect({20.0F, 20.0F, 80.0F, 80.0F}, math::Color(0.2F, 0.4F, 0.8F, 1.0F));
        renderer.endGroup();
        renderer.drawRect({60.0F, 60.0F, 20.0F, 20.0F}, math::Color::white(), {.layer = 2});
    });
    // clang-format on
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    // The group renders, the canvas draws offscreen up to the group, copies what it drew, goes on in a pass that keeps it and reaches the screen.
    EXPECT_EQ(stats.passes, 5U);
}

TEST_F(GroupTest, KeepsTheModesThatReadWhatLiesBelowForGroups) {
    std::vector<std::string> errors;
    // clang-format off
    test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        const auto expect = [&errors](const std::function<void()>& call) {
            try {
                call();
                errors.emplace_back();
            } catch (const std::exception& error) {
                errors.emplace_back(error.what());
            }
        };
        renderer.beginWorld(camera, {.ambientLight = math::Color(0.5F, 0.5F, 0.5F, 1.0F)});
        expect([&renderer] { renderer.drawRect({0.0F, 0.0F, 8.0F, 8.0F}, math::Color::white(), {.blend = graphics::BlendMode::Type::Difference}); });
        renderer.beginGroup({.order = {.blend = graphics::BlendMode::Type::SoftLight}});
        renderer.drawRect({0.0F, 0.0F, 8.0F, 8.0F}, math::Color::white());
        expect([&renderer] { renderer.endGroup(); });
    });
    // clang-format on
    ASSERT_EQ(errors.size(), 2U);
    EXPECT_EQ(errors[0], "The blend mode \"difference\" mixes with what lies below, which only groups do. Draw inside \"beginGroup\" with that blend instead.");
    EXPECT_EQ(errors[1], "A group that blends with what lies below draws in a canvas without ambient light.");
}

TEST_F(GroupTest, DrawsTheSameGroupsEveryFrameWithoutAllocating) {
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        Renderer& renderer = engine.getRenderer2D();
        renderer.beginWorld(camera);
        renderer.pushClip({0.0F, 0.0F, 100.0F, 100.0F});
        renderer.pushMask(Renderer::MaskMode::Inside, {});
        renderer.drawCircle({0.0F, 0.0F}, 30.0F, math::Color::white());
        renderer.endMaskShape();
        renderer.beginGroup({.opacity = 0.5F});
        renderer.drawRect({0.0F, 0.0F, 50.0F, 50.0F}, math::Color::white());
        renderer.endGroup();
        renderer.popMask();
        renderer.popClip();
        renderer.beginGroup({.order = {.blend = graphics::BlendMode::Type::Multiply}});
        renderer.drawRect({10.0F, 10.0F, 50.0F, 50.0F}, math::Color::white());
        renderer.endGroup();
        renderer.beginGroup({.order = {.blend = graphics::BlendMode::Type::ColorBurn}});
        renderer.drawRect({10.0F, 10.0F, 50.0F, 50.0F}, math::Color::white());
        renderer.endGroup();
    }));
    // clang-format on
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    const test::AllocationTracker tracker;
    fixture.frames(5);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

} // namespace haylen::graphics2d
