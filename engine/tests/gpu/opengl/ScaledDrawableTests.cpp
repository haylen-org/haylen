#include <gtest/gtest.h>

#include <stdexcept>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/OpenGlTest.hpp"
#include "gpu/opengl/ScaledDrawable.hpp"

namespace haylen::gpu::opengl {

class ScaledDrawableTest : public OpenGlTest {
  protected:
    static constexpr Surface::Size kDrawable{.width = 32, .height = 32, .windowWidth = kSize, .windowHeight = kSize};
    static constexpr Surface::Size kResized{.width = 16, .height = 16, .windowWidth = kSize, .windowHeight = kSize};

    // The answer of a context that cannot draw into the framebuffer it was handed.
    static Gl::Enum refuseFramebuffer(Gl::Enum) {
        return 0;
    }

    void SetUp() override {
        OpenGlTest::SetUp();

        if (IsSkipped()) {
            return;
        }

        opened.gl.load(*context, GetParam());
        opened.state.reset();
    }

    Context opened;
};

// A size whose framebuffer the context refused keeps no framebuffer, so the next frame that asks for that size makes it again.
TEST_P(ScaledDrawableTest, MakesTheFramebufferOfASizeAgainAfterTheContextRefusedIt) {
    ScaledDrawable drawable(opened.gl, opened.state, kDrawable, 1);
    const auto checkFramebufferStatus = opened.gl.checkFramebufferStatus;

    opened.gl.checkFramebufferStatus = &refuseFramebuffer;
    EXPECT_THROW(drawable.resize(kResized), std::runtime_error);
    EXPECT_EQ(drawable.getFramebuffer(), 0U);

    opened.gl.checkFramebufferStatus = checkFramebufferStatus;
    drawable.resize(kResized);
    EXPECT_NE(drawable.getFramebuffer(), 0U);
}

INSTANTIATE_TEST_SUITE_P(Flavours, ScaledDrawableTest, ::testing::Values(Device::Api::OpenGl, Device::Api::OpenGlEs), OpenGlTest::getFlavourName);

} // namespace haylen::gpu::opengl
