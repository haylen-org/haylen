#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/DrawnClear.hpp"
#include "gpu/opengl/OpenGlTest.hpp"

namespace haylen::gpu::opengl {

class DrawnClearTest : public OpenGlTest {
  protected:
    static constexpr int kTargetSize = 4;

    void SetUp() override {
        OpenGlTest::SetUp();

        if (IsSkipped()) {
            return;
        }

        if (GetParam() != Device::Api::OpenGlEs) {
            GTEST_SKIP() << "The drawn clear serves OpenGL ES drivers alone.";
        }

        opened.gl.load(*context, GetParam());
        opened.state.reset();

        Gl& gl = opened.gl;
        gl.genTextures(static_cast<Gl::Sizei>(targets.size()), targets.data());

        for (const Gl::Uint target : targets) {
            opened.state.bindTexture(0, Gl::kTexture2d, target);
            gl.texStorage2D(Gl::kTexture2d, 1, Gl::kRgba8, kTargetSize, kTargetSize);
        }

        gl.genFramebuffers(1, &framebuffer);
        opened.state.bindDrawFramebuffer(framebuffer);

        for (std::size_t index = 0; index < targets.size(); ++index) {
            gl.framebufferTexture2D(Gl::kDrawFramebuffer, Gl::kColorAttachment0 + static_cast<Gl::Enum>(index), Gl::kTexture2d, targets[index], 0);
        }

        const std::array<Gl::Enum, 2> buffers{Gl::kColorAttachment0, Gl::kColorAttachment0 + 1};
        gl.drawBuffers(static_cast<Gl::Sizei>(buffers.size()), buffers.data());
    }

    void TearDown() override {
        if (framebuffer != 0) {
            opened.gl.deleteFramebuffers(1, &framebuffer);
            opened.state.forgetFramebuffer(framebuffer);
            opened.gl.deleteTextures(static_cast<Gl::Sizei>(targets.size()), targets.data());
        }

        OpenGlTest::TearDown();
    }

    void clearBoth(const std::array<Gl::Float, 4>& color) {
        opened.state.prepareClear();
        opened.gl.clearBufferfv(Gl::kColor, 0, color.data());
        opened.gl.clearBufferfv(Gl::kColor, 1, color.data());
    }

    [[nodiscard]] std::uint32_t readCorner(std::size_t attachment) {
        opened.state.bindReadFramebuffer(framebuffer);
        opened.gl.readBuffer(Gl::kColorAttachment0 + static_cast<Gl::Enum>(attachment));
        std::uint32_t pixel = 0;
        opened.gl.readPixels(kTargetSize - 1, kTargetSize - 1, 1, 1, Gl::kRgba, Gl::kUnsignedByte, &pixel);
        return pixel;
    }

    Context opened;
    std::array<Gl::Uint, 2> targets{};
    Gl::Uint framebuffer = 0;
};

// The draw paints the whole attachment it names, leaves the other attachment as it was and gives the framebuffer back the draw buffer of every attachment, which the clears that follow reach.
TEST_P(DrawnClearTest, PaintsOnlyTheAttachmentItNames) {
    constexpr std::uint32_t kBlue = 0xFFFF0000;
    constexpr std::uint32_t kRed = 0xFF0000FF;
    constexpr std::uint32_t kGreen = 0xFF00FF00;
    clearBoth({0.0F, 0.0F, 1.0F, 1.0F});

    DrawnClear clear(opened.gl, opened.state);
    clear.clear(1, targets.size(), {1.0F, 0.0F, 0.0F, 1.0F}, kTargetSize, kTargetSize);
    EXPECT_EQ(readCorner(0), kBlue);
    EXPECT_EQ(readCorner(1), kRed);

    clearBoth({0.0F, 1.0F, 0.0F, 1.0F});
    EXPECT_EQ(readCorner(0), kGreen);
    EXPECT_EQ(readCorner(1), kGreen);
}

// The draw paints every sample of a multisampled attachment, so its resolve reads the whole clear color.
TEST_P(DrawnClearTest, PaintsEverySampleOfAMultisampledAttachment) {
    constexpr Gl::Sizei kSamples = 4;
    constexpr std::uint32_t kRed = 0xFF0000FF;
    Gl& gl = opened.gl;
    Gl::Uint samples = 0;
    gl.genRenderbuffers(1, &samples);
    gl.bindRenderbuffer(Gl::kRenderbuffer, samples);
    gl.renderbufferStorageMultisample(Gl::kRenderbuffer, kSamples, Gl::kRgba8, kTargetSize, kTargetSize);
    Gl::Uint multisampled = 0;
    gl.genFramebuffers(1, &multisampled);
    opened.state.bindDrawFramebuffer(multisampled);
    gl.framebufferRenderbuffer(Gl::kDrawFramebuffer, Gl::kColorAttachment0, Gl::kRenderbuffer, samples);

    DrawnClear clear(opened.gl, opened.state);
    clear.clear(0, 1, {1.0F, 0.0F, 0.0F, 1.0F}, kTargetSize, kTargetSize);

    // The resolve writes the first attachment of the fixture, whose corner then holds the mean of the samples.
    opened.state.bindReadFramebuffer(multisampled);
    opened.state.bindDrawFramebuffer(framebuffer);
    const std::array<Gl::Enum, 1> resolved{Gl::kColorAttachment0};
    gl.drawBuffers(static_cast<Gl::Sizei>(resolved.size()), resolved.data());
    gl.blitFramebuffer(0, 0, kTargetSize, kTargetSize, 0, 0, kTargetSize, kTargetSize, Gl::kColorBufferBit, Gl::kNearest);
    EXPECT_EQ(readCorner(0), kRed);

    gl.deleteFramebuffers(1, &multisampled);
    opened.state.forgetFramebuffer(multisampled);
    gl.deleteRenderbuffers(1, &samples);
}

INSTANTIATE_TEST_SUITE_P(Flavours, DrawnClearTest, ::testing::Values(Device::Api::OpenGl, Device::Api::OpenGlEs), OpenGlTest::getFlavourName);

} // namespace haylen::gpu::opengl
