#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/Device.hpp"
#include "gpu/opengl/OpenGlTest.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::gpu::opengl {

class OpenGlDeviceTest : public OpenGlTest {
  protected:
    static constexpr Gl::Enum kResetNotificationStrategy = 0x8256;
    static constexpr Gl::Int kLoseContextOnReset = 0x8252;
    static constexpr Gl::Enum kTexture2D = 0x0DE1;

    void SetUp() override {
        OpenGlTest::SetUp();
        if (IsSkipped()) {
            return;
        }
        device = Device::create(GetParam(), context->getSurface(), {.validation = true});
        const std::string header = GetParam() == Device::Api::OpenGl ? "#version 430\n" : "#version 300 es\nprecision highp float;\n";
        vertex = header + "uniform vec4 shift[1];\nin vec2 position;\nin vec2 offset;\nvoid main() { gl_Position = vec4(position + offset + shift[0].xy, 0.0, 1.0); }\n";
        fragment = header + "uniform highp sampler2D picture;\nout vec4 color;\nvoid main() { color = texture(picture, vec2(0.5)); }\n";
    }

    void TearDown() override {
        device.reset();
        OpenGlTest::TearDown();
    }

    // Makes what a frame of sprites needs: a program with a block and a texture, a pipeline with an instanced buffer, a dynamic buffer and image, and a render target.
    void makeScene() {
        const std::array<Device::Attribute, 2> attributes{{{.location = 0, .name = "position"}, {.location = 1, .name = "offset"}}};
        const std::array<Device::BlockBinding, 1> blocks{{{.slot = 0, .stage = Device::Stage::Vertex, .size = 16, .glslName = "shift", .glslCount = 1}}};
        const std::array<Device::TextureBinding, 1> textures{{{.slot = 0}}};
        const std::array<Device::SamplerBinding, 1> samplers{{{.slot = 0}}};
        const std::array<Device::TexturePair, 1> pairs{{{.texture = 0, .sampler = 0, .glslName = "picture"}}};
        program = device->makeProgram({.vertex = {.source = vertex}, .fragment = {.source = fragment}, .attributes = attributes, .blocks = blocks, .textures = textures, .samplers = samplers, .pairs = pairs, .label = "scene"});

        const std::array<Device::VertexBufferLayout, 2> layouts{{{.stride = 8}, {.stride = 8, .perInstance = true}}};
        const std::array<Device::VertexAttribute, 2> inputs{{{.location = 0, .buffer = 0, .format = Device::VertexFormat::Float2}, {.location = 1, .buffer = 1, .format = Device::VertexFormat::Float2}}};
        const std::array<Device::ColorTarget, 1> colors{};
        pipeline = device->makePipeline({.program = program, .buffers = layouts, .attributes = inputs, .colors = colors, .label = "scene"});

        const std::array<float, 6> corners{-1.0F, -1.0F, 1.0F, -1.0F, 0.0F, 1.0F};
        quad = device->makeBuffer({.data = std::as_bytes(std::span(corners)), .label = "corners"});
        instances = device->makeBuffer({.size = 64, .usage = Device::Usage::Dynamic, .label = "instances"});
        image = device->makeImage({.width = 2, .height = 2, .kind = Device::ImageKind::Dynamic, .label = "picture"});
        sampler = device->makeSampler({});
        target = device->makeImage({.width = 16, .height = 16, .kind = Device::ImageKind::RenderTarget, .label = "target"});
    }

    void drawFrame() {
        const std::array<float, 4> offsets{0.0F, 0.0F, 0.25F, 0.0F};
        const std::array<std::byte, 16> pixels{};
        const std::array<Device::ImageWrite, 1> writes{{{.image = image, .pixels = pixels}}};
        device->writeBuffer(instances, 0, std::as_bytes(std::span(offsets)));
        device->writeImages(writes);

        const std::array<Device::ColorAttachment, 1> attachments{{{.image = target}}};
        device->beginPass({.colors = attachments, .label = "scene"});
        device->applyPipeline(pipeline);
        Device::Bindings bindings;
        bindings.vertexBuffers[0] = quad;
        bindings.vertexBuffers[1] = instances;
        bindings.textures[0] = image;
        bindings.samplers[0] = sampler;
        for (int offset = 0; offset < 16; offset += 8) {
            bindings.vertexOffsets[1] = offset;
            device->applyBindings(bindings);
            device->applyUniforms(0, std::as_bytes(std::span(offsets)));
            device->draw(0, 3, 1);
        }
        device->endPass();
        device->beginPass({});
        device->endPass();
        device->commit();
    }

    std::unique_ptr<gpu::Device> device;
    std::string vertex;
    std::string fragment;
    Device::Program program;
    Device::Pipeline pipeline;
    Device::Buffer quad;
    Device::Buffer instances;
    Device::Image image;
    Device::Sampler sampler;
    Device::Image target;
};

TEST_P(OpenGlDeviceTest, StartsOnlyOnAnOpenGlSurface) {
    try {
        static_cast<void>(Device::create(GetParam(), std::nullopt, {}));
        FAIL() << "The device started without a context.";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "The OpenGL device draws through an OpenGL context, and the platform gave it no surface with one.");
    }
}

TEST_P(OpenGlDeviceTest, RefusesAContextOfTheOtherFlavour) {
    const Device::Api other = GetParam() == Device::Api::OpenGl ? Device::Api::OpenGlEs : Device::Api::OpenGl;
    EXPECT_THROW(static_cast<void>(Device::create(other, context->getSurface(), {})), std::runtime_error);
}

TEST_P(OpenGlDeviceTest, ReportsTheBottomRowAsTheFirstRow) {
    EXPECT_FALSE(device->getCapabilities().originTopLeft);
    EXPECT_EQ(device->getApi(), GetParam());
    EXPECT_EQ(device->getLanguage(), GetParam() == Device::Api::OpenGl ? Device::Language::Glsl430 : Device::Language::Glsl300es);
}

// The context of the tests and the benchmarks is lost when the GPU resets, as the contexts of apps are, so the device sees the reset instead of waiting for frames that the GPU never finishes.
TEST_P(OpenGlDeviceTest, DrawsThroughAContextThatIsLostWhenTheGpuResets) {
    Context opened;
    opened.gl.load(*context, GetParam());
    if (!opened.gl.getFeatures().robustness) {
        GTEST_SKIP() << "The context reports no reset of the GPU.";
    }
    Gl::Int strategy = 0;
    opened.gl.getIntegerv(kResetNotificationStrategy, &strategy);
    EXPECT_EQ(strategy, kLoseContextOnReset);
}

// A texture names its last level, so a driver that ignores the level count of immutable storage still samples a texture of one level as complete.
TEST_P(OpenGlDeviceTest, LimitsTheLevelsOfATextureToTheLevelsItHas) {
    const Device::Image image = device->makeImage({.width = 4, .height = 4, .kind = Device::ImageKind::Dynamic, .label = "level-count"});
    Context opened;
    opened.gl.load(*context, GetParam());
    Gl::Int lastLevel = -1;
    opened.gl.getTexParameteriv(kTexture2D, Gl::kTextureMaxLevel, &lastLevel);
    EXPECT_EQ(lastLevel, 0);
    device->destroy(image);
}

// The objects of the device live in its context, so a surface that brings another context cannot take the place of the one it lost.
TEST_P(OpenGlDeviceTest, TakesBackOnlyTheContextItWasMadeWith) {
    device->detachSurface();
    {
        test::OffscreenGlContext other(GetParam(), kSize, kSize);
        EXPECT_THROW(device->attachSurface(other.getSurface()), std::logic_error);
    }
    device->attachSurface(context->getSurface());
}

TEST_P(OpenGlDeviceTest, AllocatesNothingForAFrameThatRepeatsTheOneBeforeIt) {
    makeScene();
    for (std::size_t frame = 0; frame < Device::kFramesInFlight + 1; ++frame) {
        drawFrame();
    }
    const test::AllocationTracker tracker;
    drawFrame();
    EXPECT_EQ(tracker.getTotal(), 0U);
}

// A frame without a surface, such as an Android app in the background, draws its offscreen passes and records nothing into the swapchain.
TEST_P(OpenGlDeviceTest, DrawsOffscreenWhileTheSurfaceIsAway) {
    makeScene();
    device->detachSurface();
    drawFrame();
    device->attachSurface(context->getSurface());
    drawFrame();
}

INSTANTIATE_TEST_SUITE_P(Flavours, OpenGlDeviceTest, ::testing::Values(Device::Api::OpenGl, Device::Api::OpenGlEs), OpenGlTest::getFlavourName);

} // namespace haylen::gpu::opengl
