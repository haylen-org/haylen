#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::gpu {

class NullDeviceTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Device::Image makeDynamic(NullDevice& device) {
        return device.makeImage({.width = 2, .height = 2, .kind = Device::ImageKind::Dynamic, .label = "dynamic"});
    }

    // Draws one pass into the swapchain with a pipeline that writes the color channels.
    static void drawFrame(NullDevice& device, Device::Pipeline pipeline) {
        device.beginPass({.load = Device::Load::Clear, .clear = math::Color::white()});
        device.applyPipeline(pipeline);
        device.draw(0, 3, 1);
        device.endPass();
        device.commit();
    }

    std::array<std::byte, 16> pixels{};
};

// A device stages every image of a frame before it copies any, so a frame writes its images in one call before its first pass.
TEST_F(NullDeviceTest, WritesTheImagesOfAFrameInOneCallBeforeItsPasses) {
    NullDevice device;
    const Device::Image image = makeDynamic(device);
    const std::array<Device::ImageWrite, 1> writes{{{.image = image, .pixels = pixels}}};
    device.writeImages(writes);
    EXPECT_THROW(device.writeImages(writes), std::logic_error);
    device.commit();
    EXPECT_EQ(device.getLastFrame().imageWrites, 1);
    EXPECT_EQ(device.getLastFrame().imageWriteBytes, pixels.size());

    device.beginPass({});
    device.endPass();
    EXPECT_THROW(device.writeImages(writes), std::logic_error);
    device.commit();
    device.writeImages(writes);

    const Device::Image texture = device.makeImage({.width = 2, .height = 2, .levels = std::array<std::span<const std::byte>, 1>{pixels}, .label = "texture"});
    const std::array<Device::ImageWrite, 1> fixed{{{.image = texture, .pixels = pixels}}};
    device.commit();
    EXPECT_THROW(device.writeImages(fixed), std::logic_error);
}

// The device holds room for a bounded number of objects of each kind, like a real one, and reports it.
TEST_F(NullDeviceTest, RunsOutOfRoomLikeARealDevice) {
    NullDevice device;
    const Device::Program program = device.makeProgram({.label = "empty"});
    const std::array<Device::ColorTarget, 1> colors{};
    std::vector<Device::Pipeline> pipelines;
    std::string error;
    try {
        while (pipelines.size() <= static_cast<std::size_t>(NullDevice::kPipelinePoolSize)) {
            pipelines.push_back(device.makePipeline({.program = program, .colors = colors, .label = "empty"}));
        }
    } catch (const std::runtime_error& failure) {
        error = failure.what();
    }
    EXPECT_EQ(error, "The graphics device has no room for another pipeline. At most 2048 pipelines can exist at once.");
    EXPECT_EQ(pipelines.size(), static_cast<std::size_t>(NullDevice::kPipelinePoolSize));
    for (const Device::Pool& pool : device.getPools()) {
        if (pool.name == "pipelines") {
            EXPECT_EQ(pool.used, NullDevice::kPipelinePoolSize);
        }
    }

    device.destroy(pipelines.back());
    EXPECT_NO_THROW((void)device.makePipeline({.program = program, .colors = colors, .label = "empty"}));
}

// A device without independent blending applies the blend and the write mask of the first color to every color, so the engine never asks it for different ones, and a device with it takes them.
TEST_F(NullDeviceTest, RejectsPipelinesWhoseColorsBlendApartWithoutIndependentBlending) {
    NullDevice device;
    const Device::Program program = device.makeProgram({.label = "empty"});
    const Device::Blend adding{.enabled = true, .sourceColor = Device::BlendFactor::SourceAlpha, .destinationColor = Device::BlendFactor::One};
    const std::array<Device::ColorTarget, 2> alike{{{.blend = adding}, {.blend = adding}}};
    EXPECT_NO_THROW((void)device.makePipeline({.program = program, .colors = alike, .label = "alike"}));
    const std::array<Device::ColorTarget, 2> blends{{{.blend = adding}, {}}};
    EXPECT_THROW((void)device.makePipeline({.program = program, .colors = blends, .label = "blends"}), std::logic_error);
    const std::array<Device::ColorTarget, 2> masks{{{.blend = adding}, {.writeMask = Device::ColorMask::None, .blend = adding}}};
    EXPECT_THROW((void)device.makePipeline({.program = program, .colors = masks, .label = "masks"}), std::logic_error);

    Device::Capabilities capabilities = NullDevice::getDefaultCapabilities();
    capabilities.independentBlend = true;
    NullDevice independent(Device::Language::None, capabilities);
    const Device::Program made = independent.makeProgram({.label = "empty"});
    EXPECT_NO_THROW((void)independent.makePipeline({.program = made, .colors = blends, .label = "blends"}));

    const std::array<Device::ColorTarget, 1> wide{{{.format = Device::PixelFormat::Rgba32Float, .blend = adding}}};
    EXPECT_THROW((void)device.makePipeline({.program = program, .colors = wide, .label = "wide"}), std::logic_error);
}

// Every draw needs a pass and a pipeline, and the record of the last frame says what each pass cleared to and wrote.
TEST_F(NullDeviceTest, ChecksDrawsAndRecordsTheFramesWithoutAllocatingOnceSteady) {
    NullDevice device;
    EXPECT_THROW(device.draw(0, 3, 1), std::logic_error);
    const Device::Program program = device.makeProgram({.label = "empty"});
    const std::array<Device::ColorTarget, 1> colors{{{.writeMask = Device::ColorMask::Rgb}}};
    const Device::Pipeline pipeline = device.makePipeline({.program = program, .colors = colors, .swapchain = true, .label = "swapchain"});
    device.beginPass({});
    EXPECT_THROW(device.beginPass({}), std::logic_error);
    EXPECT_THROW(device.draw(0, 3, 1), std::logic_error);
    device.endPass();
    EXPECT_THROW(device.applyPipeline(pipeline), std::logic_error);
    device.commit();

    drawFrame(device, pipeline);
    ASSERT_EQ(device.getLastFrame().passes.size(), 1U);
    const NullDevice::PassRecord& pass = device.getLastFrame().passes.front();
    EXPECT_TRUE(pass.swapchain);
    EXPECT_EQ(pass.clear, math::Color::white());
    ASSERT_EQ(pass.masks.size(), 1U);
    EXPECT_EQ(pass.masks.front(), Device::ColorMask::Rgb);
    EXPECT_EQ(pass.draws, 1);

    const test::AllocationTracker tracker;
    for (int frame = 0; frame < 4; ++frame) {
        drawFrame(device, pipeline);
    }
    EXPECT_EQ(tracker.getTotal(), 0U);
}

// Images fit their type, layers, mip levels and sample count, textures bring the pixels of every layer of every level, and depth formats belong to depth stencil images alone.
TEST_F(NullDeviceTest, ChecksTheShapeLevelsAndFormatOfImages) {
    NullDevice device;
    EXPECT_THROW((void)device.makeImage({.width = 2, .height = 2, .layers = 2, .kind = Device::ImageKind::RenderTarget, .label = "flat"}), std::logic_error);
    EXPECT_THROW((void)device.makeImage({.type = Device::TextureType::Cube, .width = 2, .height = 2, .layers = 4, .kind = Device::ImageKind::RenderTarget, .label = "cube"}), std::logic_error);
    EXPECT_THROW((void)device.makeImage({.width = 4, .height = 4, .mipLevels = 4, .kind = Device::ImageKind::RenderTarget, .label = "levels"}), std::logic_error);
    EXPECT_THROW((void)device.makeImage({.width = 4, .height = 4, .sampleCount = 3, .kind = Device::ImageKind::RenderTarget, .label = "samples"}), std::logic_error);
    EXPECT_THROW((void)device.makeImage({.width = 4, .height = 4, .sampleCount = 4, .kind = Device::ImageKind::Dynamic, .label = "multisampled"}), std::logic_error);
    EXPECT_THROW((void)device.makeImage({.width = 4, .height = 4, .format = Device::PixelFormat::Depth32Float, .kind = Device::ImageKind::RenderTarget, .label = "depth"}), std::logic_error);
    EXPECT_THROW((void)device.makeImage({.width = 4, .height = 4, .kind = Device::ImageKind::DepthStencil, .label = "color"}), std::logic_error);
    EXPECT_NO_THROW((void)device.makeImage({.width = 4, .height = 4, .mipLevels = 3, .kind = Device::ImageKind::RenderTarget, .label = "levels"}));
    EXPECT_NO_THROW((void)device.makeImage({.width = 4, .height = 4, .sampleCount = 4, .format = Device::PixelFormat::Depth24Stencil8, .kind = Device::ImageKind::DepthStencil, .label = "depth"}));

    // An array keeps its layers at every level, and a volume halves its slices with its sides.
    const std::vector<std::byte> arrayBase(2 * 2 * 3 * 4);
    const std::vector<std::byte> arrayTop(1 * 1 * 3 * 4);
    const std::array<std::span<const std::byte>, 2> arrayLevels{arrayBase, arrayTop};
    EXPECT_NO_THROW((void)device.makeImage({.type = Device::TextureType::Array, .width = 2, .height = 2, .layers = 3, .mipLevels = 2, .levels = arrayLevels, .label = "array"}));
    const std::vector<std::byte> volumeBase(2 * 2 * 2 * 4);
    const std::vector<std::byte> volumeTop(1 * 1 * 1 * 4);
    const std::array<std::span<const std::byte>, 2> volumeLevels{volumeBase, volumeTop};
    EXPECT_NO_THROW((void)device.makeImage({.type = Device::TextureType::Volume, .width = 2, .height = 2, .layers = 2, .mipLevels = 2, .levels = volumeLevels, .label = "volume"}));
    EXPECT_THROW((void)device.makeImage({.type = Device::TextureType::Volume, .width = 2, .height = 2, .layers = 2, .mipLevels = 2, .levels = arrayLevels, .label = "volume"}), std::logic_error);
    EXPECT_THROW((void)device.makeImage({.width = 2, .height = 2, .mipLevels = 1, .levels = volumeLevels, .label = "texture"}), std::logic_error);
}

// The attachments of a pass share one size and sample count, a multisampled color resolves into a render target of one sample of its format, and every pipeline drawn in a pass draws into its attachments.
TEST_F(NullDeviceTest, ChecksTheAttachmentsOfPassesAndThePipelinesDrawnInThem) {
    NullDevice device;
    const Device::Image samples = device.makeImage({.width = 8, .height = 8, .sampleCount = 4, .kind = Device::ImageKind::RenderTarget, .label = "samples"});
    const Device::Image resolved = device.makeImage({.width = 8, .height = 8, .kind = Device::ImageKind::RenderTarget, .label = "resolved"});
    const Device::Image halfFloat = device.makeImage({.width = 8, .height = 8, .format = Device::PixelFormat::Rgba16Float, .kind = Device::ImageKind::RenderTarget, .label = "half"});
    const Device::Image depth = device.makeImage({.width = 8, .height = 8, .sampleCount = 4, .format = Device::PixelFormat::Depth32Float, .kind = Device::ImageKind::DepthStencil, .label = "depth"});
    const Device::Image small = device.makeImage({.width = 4, .height = 4, .sampleCount = 4, .format = Device::PixelFormat::Depth32Float, .kind = Device::ImageKind::DepthStencil, .label = "small"});

    const std::array<Device::ColorAttachment, 1> wrongResolve{{{.image = samples, .resolve = halfFloat}}};
    EXPECT_THROW(device.beginPass({.colors = wrongResolve}), std::logic_error);
    const std::array<Device::ColorAttachment, 1> singleResolve{{{.image = resolved, .resolve = halfFloat}}};
    EXPECT_THROW(device.beginPass({.colors = singleResolve}), std::logic_error);
    const std::array<Device::ColorAttachment, 1> colors{{{.image = samples, .store = Device::Store::Discard, .resolve = resolved}}};
    EXPECT_THROW(device.beginPass({.colors = colors, .depthStencil = Device::DepthStencilAttachment{.image = small}}), std::logic_error);

    const Device::Program program = device.makeProgram({.label = "empty"});
    const std::array<Device::ColorTarget, 1> targets{};
    const Device::Pipeline single = device.makePipeline({.program = program, .colors = targets, .depthStencil = {.format = Device::PixelFormat::Depth32Float}, .label = "single"});
    const Device::Pipeline matching = device.makePipeline({.program = program, .colors = targets, .depthStencil = {.format = Device::PixelFormat::Depth32Float}, .sampleCount = 4, .label = "matching"});
    const Device::Pipeline swapchain = device.makePipeline({.program = program, .colors = targets, .swapchain = true, .label = "swapchain"});
    device.beginPass({.colors = colors, .depthStencil = Device::DepthStencilAttachment{.image = depth}});
    EXPECT_THROW(device.applyPipeline(single), std::logic_error);
    EXPECT_THROW(device.applyPipeline(swapchain), std::logic_error);
    EXPECT_NO_THROW(device.applyPipeline(matching));
    device.endPass();
    device.commit();
}

// A draw samples no attachment of its own pass, takes images of the type and kind of its texture slots, and reads the bindings and every uniform block that came after its pipeline.
TEST_F(NullDeviceTest, RequiresTheBindingsAndUniformsOfEveryDraw) {
    NullDevice device;
    const std::array<Device::TextureBinding, 1> textures{{{.slot = 0}}};
    const std::array<Device::SamplerBinding, 1> samplers{{{.slot = 0}}};
    const std::array<Device::BlockBinding, 1> blocks{{{.slot = 0, .size = 16}}};
    const Device::Program program = device.makeProgram({.blocks = blocks, .textures = textures, .samplers = samplers, .label = "textured"});
    const std::array<Device::ColorTarget, 1> targets{};
    const Device::Pipeline pipeline = device.makePipeline({.program = program, .colors = targets, .label = "textured"});
    const Device::Image target = device.makeImage({.width = 2, .height = 2, .kind = Device::ImageKind::RenderTarget, .label = "target"});
    const Device::Image other = device.makeImage({.width = 2, .height = 2, .kind = Device::ImageKind::RenderTarget, .label = "other"});
    const Device::Sampler sampler = device.makeSampler({});
    const Device::Sampler comparing = device.makeSampler({.compare = Device::CompareFunction::Less});
    const std::array<Device::ColorAttachment, 1> colors{{{.image = target}}};

    device.beginPass({.colors = colors});
    device.applyPipeline(pipeline);
    Device::Bindings bindings;
    bindings.textures[0] = target;
    bindings.samplers[0] = sampler;
    EXPECT_THROW(device.applyBindings(bindings), std::logic_error);
    bindings.textures[0] = other;
    bindings.samplers[0] = comparing;
    EXPECT_THROW(device.applyBindings(bindings), std::logic_error);
    bindings.samplers[0] = sampler;
    device.applyBindings(bindings);
    EXPECT_THROW(device.draw(0, 3, 1), std::logic_error);
    device.applyUniforms(0, pixels);
    EXPECT_NO_THROW(device.draw(0, 3, 1));
    device.applyPipeline(pipeline);
    EXPECT_THROW(device.draw(0, 3, 1), std::logic_error);
    device.endPass();
    device.commit();
}

// A frame writes each part of a dynamic buffer once and before a pass binds it, and an indexed draw reads only the indices its buffer holds.
TEST_F(NullDeviceTest, WritesBuffersBeforeTheirPassesAndDrawsWithinTheirIndices) {
    NullDevice device;
    const Device::Buffer vertices = device.makeBuffer({.size = 64, .usage = Device::Usage::Dynamic, .label = "vertices"});
    const Device::Buffer indices = device.makeBuffer({.size = 12, .usage = Device::Usage::Dynamic, .index = true, .label = "indices"});
    device.writeBuffer(vertices, 0, pixels);
    device.writeBuffer(vertices, 16, pixels);
    EXPECT_THROW(device.writeBuffer(vertices, 8, pixels), std::logic_error);

    const Device::Program program = device.makeProgram({.label = "empty"});
    const std::array<Device::VertexBufferLayout, 1> layouts{{{.stride = 8}}};
    const std::array<Device::ColorTarget, 1> targets{};
    const Device::Pipeline pipeline = device.makePipeline({.program = program, .buffers = layouts, .indexType = Device::IndexType::UInt16, .colors = targets, .swapchain = true, .label = "indexed"});
    device.beginPass({});
    device.applyPipeline(pipeline);
    Device::Bindings bindings;
    bindings.vertexBuffers[0] = vertices;
    bindings.indexBuffer = indices;
    bindings.indexOffset = 2;
    device.applyBindings(bindings);
    EXPECT_NO_THROW(device.draw(0, 5, 1));
    EXPECT_THROW(device.draw(1, 5, 1), std::logic_error);
    device.endPass();
    EXPECT_THROW(device.writeBuffer(vertices, 32, pixels), std::logic_error);
    device.commit();
    EXPECT_NO_THROW(device.writeBuffer(vertices, 0, pixels));
}

// A debug group pushed inside a pass closes in that pass, one pushed between passes closes between them, and a frame commits with none open.
TEST_F(NullDeviceTest, BalancesDebugGroupsWithinTheirPasses) {
    NullDevice device;
    EXPECT_THROW(device.popDebugGroup(), std::logic_error);
    device.pushDebugGroup("frame");
    device.beginPass({});
    EXPECT_THROW(device.popDebugGroup(), std::logic_error);
    device.pushDebugGroup("sprites");
    EXPECT_THROW(device.endPass(), std::logic_error);
    device.popDebugGroup();
    device.endPass();
    EXPECT_THROW(device.commit(), std::logic_error);
    device.popDebugGroup();
    EXPECT_NO_THROW(device.commit());
}

// The surface goes and comes back in turn between frames, and a minimized window resizes it to no pixels.
TEST_F(NullDeviceTest, TakesTheSurfaceInTurnBetweenFrames) {
    NullDevice device;
    EXPECT_THROW(device.attachSurface({.width = 8, .height = 8}), std::logic_error);
    device.detachSurface();
    EXPECT_THROW(device.detachSurface(), std::logic_error);
    EXPECT_THROW(device.attachSurface({.width = 8, .height = 8, .sampleCount = 3}), std::logic_error);
    device.attachSurface({.width = 8, .height = 8});
    EXPECT_NO_THROW(device.resizeSurface(0, 0));
    EXPECT_THROW(device.resizeSurface(-1, 4), std::logic_error);
    device.beginPass({});
    EXPECT_THROW(device.resizeSurface(4, 4), std::logic_error);
    EXPECT_THROW(device.detachSurface(), std::logic_error);
    device.endPass();
    device.commit();
}

// A read copies a whole render target of one sample, between passes.
TEST_F(NullDeviceTest, ReadsWholeRenderTargetsOfOneSample) {
    NullDevice device;
    const Device::Image target = device.makeImage({.width = 4, .height = 2, .kind = Device::ImageKind::RenderTarget, .label = "target"});
    const Device::Image samples = device.makeImage({.width = 4, .height = 2, .sampleCount = 4, .kind = Device::ImageKind::RenderTarget, .label = "samples"});
    EXPECT_THROW((void)device.readImage(samples, 4, 2), std::logic_error);
    EXPECT_THROW((void)device.readImage(target, 2, 2), std::logic_error);
    const std::unique_ptr<Device::Readback> copy = device.readImage(target, 4, 2);
    EXPECT_FALSE(copy->poll().has_value());
    EXPECT_EQ(copy->poll()->size(), 4U * 2U * 4U);
}

} // namespace haylen::gpu
