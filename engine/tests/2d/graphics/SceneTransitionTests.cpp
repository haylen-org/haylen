#include <gtest/gtest.h>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SceneTransition.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "support/EngineFixture.hpp"
#include "support/FrameRenderer.hpp"

namespace haylen::graphics2d {

namespace {

// Renders one frame of a transition between two images and returns how many sprites it drew.
class SceneTransitionTest : public ::testing::Test {
  protected:
    SceneTransitionTest() : outgoing(fixture.engine().getGraphics().createRenderTarget(64, 36)), incoming(fixture.engine().getGraphics().createRenderTarget(64, 36)) {}

    [[nodiscard]] std::size_t spritesAt(SceneTransition::Kind kind, float progress) {
        SceneTransition transition({.kind = kind});
        const core::TransitionEffect::Frames frames{.outgoing = outgoing.getTexture(), .incoming = incoming.getTexture()};
        return test::FrameRenderer::renderOnce(fixture, [&](core::Engine& engine) { transition.render(engine.getRenderer2D(), frames, progress); }).sprites;
    }

    test::EngineFixture fixture;
    graphics::RenderTarget outgoing;
    graphics::RenderTarget incoming;
};

} // namespace

// An image that shrinks to nothing or grows from nothing draws nothing at that end, instead of the whole image at its own size.
TEST_F(SceneTransitionTest, DrawsNothingOfAnImageWithNoSizeLeft) {
    EXPECT_EQ(spritesAt(SceneTransition::Kind::OutIn, 1.0F), 1U);
    EXPECT_EQ(spritesAt(SceneTransition::Kind::OutIn, 0.5F), 2U);
    EXPECT_EQ(spritesAt(SceneTransition::Kind::InOut, 0.0F), 1U);
    EXPECT_EQ(spritesAt(SceneTransition::Kind::InOut, 0.5F), 2U);
}

} // namespace haylen::graphics2d
