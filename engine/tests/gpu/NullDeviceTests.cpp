#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "support/AllocationTracker.hpp"
#include "support/ContractRule.hpp"

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
    const std::array<Device::ImageWrite, 1> writes{{{.image = image, .region = {0, 0, 2, 2}, .pixels = pixels}}};
    device.writeImages(writes);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.writeImages(writes); }), gpu::ContractError::Rule::ImageWrite);
    device.commit();
    EXPECT_EQ(device.getLastFrame().imageWrites, 1);
    EXPECT_EQ(device.getLastFrame().imageWriteBytes, pixels.size());

    device.beginPass({});
    device.endPass();
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.writeImages(writes); }), gpu::ContractError::Rule::ImageWrite);
    device.commit();
    device.writeImages(writes);

    const Device::Image texture = device.makeImage({.width = 2, .height = 2, .levels = std::array<std::span<const std::byte>, 1>{pixels}, .label = "texture"});
    const std::array<Device::ImageWrite, 1> fixed{{{.image = texture, .region = {0, 0, 2, 2}, .pixels = pixels}}};
    device.commit();
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.writeImages(fixed); }), gpu::ContractError::Rule::ImageWrite);
}

// A write replaces a region inside its image with the pixels of that region alone, and each image takes one write in a call, so no copy of the call waits for another.
TEST_F(NullDeviceTest, WritesOneRegionInsideEachDynamicImage) {
    NullDevice device;
    const Device::Image image = makeDynamic(device);
    const std::span<const std::byte> texel = std::span(pixels).first(4);
    const std::array<Device::ImageWrite, 1> part{{{.image = image, .region = {1, 0, 1, 1}, .pixels = texel}}};
    device.writeImages(part);
    device.commit();
    EXPECT_EQ(device.getLastFrame().imageWriteBytes, texel.size());

    const std::array<Device::Region, 4> outside{{{2, 0, 1, 1}, {-1, 0, 1, 1}, {0, 1, 1, 2}, {0, 0, 0, 1}}};
    for (const Device::Region region : outside) {
        const std::array<Device::ImageWrite, 1> writes{{{.image = image, .region = region, .pixels = texel}}};
        EXPECT_EQ(test::ContractRule::of([&] { (void)device.writeImages(writes); }), gpu::ContractError::Rule::ImageWrite) << region.x << ", " << region.y;
        device.commit();
    }

    const std::array<Device::ImageWrite, 1> tooFewPixels{{{.image = image, .region = {0, 0, 2, 1}, .pixels = texel}}};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.writeImages(tooFewPixels); }), gpu::ContractError::Rule::ImageWrite);
    device.commit();

    const std::array<Device::ImageWrite, 2> twice{{{.image = image, .region = {0, 0, 1, 1}, .pixels = texel}, {.image = image, .region = {1, 1, 1, 1}, .pixels = texel}}};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.writeImages(twice); }), gpu::ContractError::Rule::ImageWrite);
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

    std::vector<Device::Pool> pools;
    device.readPools(pools);
    for (const Device::Pool& pool : pools) {
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
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makePipeline({.program = program, .colors = blends, .label = "blends"}); }), gpu::ContractError::Rule::Pipeline);
    const std::array<Device::ColorTarget, 2> masks{{{.blend = adding}, {.writeMask = Device::ColorMask::None, .blend = adding}}};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makePipeline({.program = program, .colors = masks, .label = "masks"}); }), gpu::ContractError::Rule::Pipeline);

    Device::Capabilities capabilities = NullDevice::getDefaultCapabilities();
    capabilities.independentBlend = true;
    NullDevice independent(Device::Language::None, capabilities);
    const Device::Program made = independent.makeProgram({.label = "empty"});
    EXPECT_NO_THROW((void)independent.makePipeline({.program = made, .colors = blends, .label = "blends"}));

    const std::array<Device::ColorTarget, 1> wide{{{.format = Device::PixelFormat::Rgba32Float, .blend = adding}}};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makePipeline({.program = program, .colors = wide, .label = "wide"}); }), gpu::ContractError::Rule::Pipeline);
}

// Every draw needs a pass and a pipeline, and the record of the last frame says what each pass cleared to and wrote.
TEST_F(NullDeviceTest, ChecksDrawsAndRecordsTheFramesWithoutAllocatingOnceSteady) {
    NullDevice device;
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.draw(0, 3, 1); }), gpu::ContractError::Rule::PassOrder);
    const Device::Program program = device.makeProgram({.label = "empty"});
    const std::array<Device::ColorTarget, 1> colors{{{.writeMask = Device::ColorMask::Rgb}}};
    const Device::Pipeline pipeline = device.makePipeline({.program = program, .colors = colors, .swapchain = true, .label = "swapchain"});
    device.beginPass({});
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.beginPass({}); }), gpu::ContractError::Rule::PassOrder);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.draw(0, 3, 1); }), gpu::ContractError::Rule::PipelineState);
    device.endPass();
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.applyPipeline(pipeline); }), gpu::ContractError::Rule::PassOrder);
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
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 2, .height = 2, .layers = 2, .kind = Device::ImageKind::RenderTarget, .label = "flat"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.type = Device::TextureType::Cube, .width = 2, .height = 2, .layers = 4, .kind = Device::ImageKind::RenderTarget, .label = "cube"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 4, .height = 4, .mipLevels = 4, .kind = Device::ImageKind::RenderTarget, .label = "levels"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 4, .height = 4, .sampleCount = 3, .kind = Device::ImageKind::RenderTarget, .label = "samples"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 4, .height = 4, .sampleCount = 4, .kind = Device::ImageKind::Dynamic, .label = "multisampled"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 4, .height = 4, .format = Device::PixelFormat::Depth32Float, .kind = Device::ImageKind::RenderTarget, .label = "depth"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 4, .height = 4, .kind = Device::ImageKind::DepthStencil, .label = "color"}); }), gpu::ContractError::Rule::Image);
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
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.type = Device::TextureType::Volume, .width = 2, .height = 2, .layers = 2, .mipLevels = 2, .levels = arrayLevels, .label = "volume"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 2, .height = 2, .mipLevels = 1, .levels = volumeLevels, .label = "texture"}); }), gpu::ContractError::Rule::Image);
}

// An image, a pipeline and a window take only the sample counts that the capabilities give their formats, and a volume and a cube only the sizes of their own limits.
TEST_F(NullDeviceTest, ChecksTheSampleCountsAndTheSizesOfEachKindOfImage) {
    Device::Capabilities capabilities = NullDevice::getDefaultCapabilities();
    capabilities.getFormat(Device::PixelFormat::R32Float).sampleCounts = 1;
    capabilities.getFormat(Device::PixelFormat::Depth24Stencil8).sampleCounts = 1 | 4;
    capabilities.maxVolumeSize = 16;
    capabilities.maxCubeSize = 32;
    NullDevice device(Device::Language::None, capabilities);

    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 4, .height = 4, .sampleCount = 4, .format = Device::PixelFormat::R32Float, .kind = Device::ImageKind::RenderTarget, .label = "floats"}); }), gpu::ContractError::Rule::Image);
    EXPECT_NO_THROW((void)device.makeImage({.width = 4, .height = 4, .sampleCount = 8, .format = Device::PixelFormat::Rgba16Float, .kind = Device::ImageKind::RenderTarget, .label = "halves"}));

    const Device::Program program = device.makeProgram({.label = "empty"});
    const std::array<Device::ColorTarget, 1> floats{{{.format = Device::PixelFormat::R32Float}}};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makePipeline({.program = program, .colors = floats, .sampleCount = 4, .label = "floats"}); }), gpu::ContractError::Rule::Pipeline);
    const std::array<Device::ColorTarget, 1> colors{};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makePipeline({.program = program, .colors = colors, .depthStencil = {.format = Device::PixelFormat::Depth24Stencil8}, .sampleCount = 2, .label = "depth"}); }), gpu::ContractError::Rule::Pipeline);

    device.detachSurface();
    const Surface::Size size{.width = 8, .height = 8, .windowWidth = 8, .windowHeight = 8};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.attachSurface({.size = size, .sampleCount = 2}); }), gpu::ContractError::Rule::Surface);
    EXPECT_NO_THROW(device.attachSurface({.size = size, .sampleCount = 4}));

    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.type = Device::TextureType::Volume, .width = 16, .height = 16, .layers = 17, .kind = Device::ImageKind::RenderTarget, .label = "volume"}); }), gpu::ContractError::Rule::Image);
    EXPECT_NO_THROW((void)device.makeImage({.type = Device::TextureType::Volume, .width = 16, .height = 16, .layers = 16, .kind = Device::ImageKind::RenderTarget, .label = "volume"}));
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.type = Device::TextureType::Cube, .width = 64, .height = 64, .layers = 6, .kind = Device::ImageKind::RenderTarget, .label = "cube"}); }), gpu::ContractError::Rule::Image);
    EXPECT_NO_THROW((void)device.makeImage({.type = Device::TextureType::Cube, .width = 32, .height = 32, .layers = 6, .kind = Device::ImageKind::RenderTarget, .label = "cube"}));
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
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.beginPass({.colors = wrongResolve}); }), gpu::ContractError::Rule::PassAttachments);
    const std::array<Device::ColorAttachment, 1> singleResolve{{{.image = resolved, .resolve = halfFloat}}};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.beginPass({.colors = singleResolve}); }), gpu::ContractError::Rule::PassAttachments);
    const std::array<Device::ColorAttachment, 1> colors{{{.image = samples, .store = Device::Store::Discard, .resolve = resolved}}};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.beginPass({.colors = colors, .depthStencil = Device::DepthStencilAttachment{.image = small}}); }), gpu::ContractError::Rule::PassAttachments);

    const Device::Program program = device.makeProgram({.label = "empty"});
    const std::array<Device::ColorTarget, 1> targets{};
    const Device::Pipeline single = device.makePipeline({.program = program, .colors = targets, .depthStencil = {.format = Device::PixelFormat::Depth32Float}, .label = "single"});
    const Device::Pipeline matching = device.makePipeline({.program = program, .colors = targets, .depthStencil = {.format = Device::PixelFormat::Depth32Float}, .sampleCount = 4, .label = "matching"});
    const Device::Pipeline swapchain = device.makePipeline({.program = program, .colors = targets, .swapchain = true, .label = "swapchain"});
    device.beginPass({.colors = colors, .depthStencil = Device::DepthStencilAttachment{.image = depth}});
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.applyPipeline(single); }), gpu::ContractError::Rule::PassAttachments);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.applyPipeline(swapchain); }), gpu::ContractError::Rule::PassAttachments);
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
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.applyBindings(bindings); }), gpu::ContractError::Rule::Bindings);
    bindings.textures[0] = other;
    bindings.samplers[0] = comparing;
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.applyBindings(bindings); }), gpu::ContractError::Rule::Bindings);
    bindings.samplers[0] = sampler;
    device.applyBindings(bindings);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.draw(0, 3, 1); }), gpu::ContractError::Rule::PipelineState);
    device.applyUniforms(0, pixels);
    EXPECT_NO_THROW(device.draw(0, 3, 1));
    device.applyPipeline(pipeline);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.draw(0, 3, 1); }), gpu::ContractError::Rule::PipelineState);
    device.endPass();
    device.commit();
}

// The image that a multisampled attachment resolves into takes the pixels of the pass as it ends, so a draw of that pass never samples it.
TEST_F(NullDeviceTest, SamplesNoImageThatThePassResolvesInto) {
    NullDevice device;
    const std::array<Device::TextureBinding, 1> textures{{{.slot = 0}}};
    const std::array<Device::SamplerBinding, 1> samplers{{{.slot = 0}}};
    const std::array<Device::ColorTarget, 1> targets{};
    const Device::Pipeline pipeline = device.makePipeline({.program = device.makeProgram({.textures = textures, .samplers = samplers, .label = "textured"}), .colors = targets, .sampleCount = 4, .label = "textured"});
    const Device::Image samples = device.makeImage({.width = 2, .height = 2, .sampleCount = 4, .kind = Device::ImageKind::RenderTarget, .label = "samples"});
    const Device::Image resolved = device.makeImage({.width = 2, .height = 2, .kind = Device::ImageKind::RenderTarget, .label = "resolved"});
    const Device::Image other = device.makeImage({.width = 2, .height = 2, .kind = Device::ImageKind::RenderTarget, .label = "other"});
    const std::array<Device::ColorAttachment, 1> colors{{{.image = samples, .resolve = resolved}}};

    device.beginPass({.colors = colors});
    device.applyPipeline(pipeline);
    Device::Bindings bindings;
    bindings.textures[0] = resolved;
    bindings.samplers[0] = device.makeSampler({});
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.applyBindings(bindings); }), gpu::ContractError::Rule::Bindings);
    bindings.textures[0] = other;
    EXPECT_NO_THROW(device.applyBindings(bindings));
    device.endPass();
    device.commit();
}

// A block compressed format makes only textures whose sides hold whole blocks and whose levels hold their rows of blocks, never an attachment or a volume, and only on a device that samples it.
TEST_F(NullDeviceTest, MakesCompressedTexturesOfWholeBlocks) {
    Device::Capabilities capabilities = NullDevice::getDefaultCapabilities();
    capabilities.getFormat(Device::PixelFormat::Astc6x6).sample = false;
    NullDevice device(Device::Language::None, capabilities);

    // A level of 8 by 8 pixels holds four blocks of BC7, and its second level of 4 by 4 pixels one block.
    const std::vector<std::byte> large(64);
    const std::vector<std::byte> small(16);
    const std::array<std::span<const std::byte>, 2> levels{large, small};
    EXPECT_NO_THROW((void)device.makeImage({.width = 8, .height = 8, .mipLevels = 2, .format = Device::PixelFormat::Bc7, .levels = levels, .label = "blocks"}));

    // The levels of an image of ASTC 5x4 round up to whole blocks, down to one block for the levels smaller than a block.
    const std::vector<std::byte> astc(4 * 16);
    const std::vector<std::byte> block(16);
    const std::array<std::span<const std::byte>, 3> astcLevels{astc, block, block};
    EXPECT_NO_THROW((void)device.makeImage({.width = 10, .height = 8, .mipLevels = 3, .format = Device::PixelFormat::Astc5x4, .levels = astcLevels, .label = "astc"}));

    const std::array<std::span<const std::byte>, 1> one{small};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 6, .height = 4, .format = Device::PixelFormat::Bc1, .levels = one, .label = "partial"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 4, .height = 4, .format = Device::PixelFormat::Bc7, .kind = Device::ImageKind::RenderTarget, .label = "target"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 4, .height = 4, .format = Device::PixelFormat::Bc7, .kind = Device::ImageKind::Dynamic, .label = "dynamic"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.type = Device::TextureType::Volume, .width = 4, .height = 4, .format = Device::PixelFormat::Bc7, .levels = one, .label = "volume"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 4, .height = 4, .format = Device::PixelFormat::Bc1, .levels = one, .label = "short"}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.makeImage({.width = 6, .height = 6, .format = Device::PixelFormat::Astc6x6, .levels = one, .label = "unsupported"}); }), gpu::ContractError::Rule::Image);
    EXPECT_FALSE(device.getCapabilities().getFormat(Device::PixelFormat::Bc7).render);
    EXPECT_EQ(device.getCapabilities().getFormat(Device::PixelFormat::Bc7).sampleCounts, 0);
}

// A slot that filters takes only images of the formats the device filters, and a sampler slot that does not filter takes no sampler with a linear filter, as WebGPU requires.
TEST_F(NullDeviceTest, FiltersOnlyTheFormatsTheDeviceFilters) {
    NullDevice device;
    EXPECT_FALSE(device.getCapabilities().getFormat(Device::PixelFormat::R32Float).filter);
    const std::array<Device::TextureBinding, 1> filtered{{{.slot = 0}}};
    const std::array<Device::TextureBinding, 1> unfiltered{{{.slot = 0, .sampleType = Device::SampleType::UnfilterableFloat}}};
    const std::array<Device::SamplerBinding, 1> filtering{{{.slot = 0}}};
    const std::array<Device::SamplerBinding, 1> nonFiltering{{{.slot = 0, .type = Device::SamplerType::NonFiltering}}};
    const std::array<Device::ColorTarget, 1> targets{};
    const Device::Pipeline filters = device.makePipeline({.program = device.makeProgram({.textures = filtered, .samplers = filtering, .label = "filters"}), .colors = targets, .label = "filters"});
    const Device::Pipeline reads = device.makePipeline({.program = device.makeProgram({.textures = unfiltered, .samplers = nonFiltering, .label = "reads"}), .colors = targets, .label = "reads"});
    const Device::Image floats = device.makeImage({.width = 2, .height = 2, .format = Device::PixelFormat::R32Float, .kind = Device::ImageKind::RenderTarget, .label = "floats"});
    const Device::Image target = device.makeImage({.width = 2, .height = 2, .kind = Device::ImageKind::RenderTarget, .label = "target"});
    const Device::Sampler linear = device.makeSampler({});
    const Device::Sampler nearest = device.makeSampler({.minification = Device::Filter::Nearest, .magnification = Device::Filter::Nearest});
    const std::array<Device::ColorAttachment, 1> colors{{{.image = target}}};

    device.beginPass({.colors = colors});
    Device::Bindings bindings;
    bindings.textures[0] = floats;
    bindings.samplers[0] = linear;
    device.applyPipeline(filters);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.applyBindings(bindings); }), gpu::ContractError::Rule::Bindings);
    device.applyPipeline(reads);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.applyBindings(bindings); }), gpu::ContractError::Rule::Bindings);
    bindings.samplers[0] = nearest;
    EXPECT_NO_THROW(device.applyBindings(bindings));
    device.endPass();
    device.commit();
}

// A frame writes each part of a dynamic buffer once, at an offset that is a multiple of four, and before a pass binds it, and an indexed draw reads only the indices its buffer holds.
TEST_F(NullDeviceTest, WritesBuffersBeforeTheirPassesAndDrawsWithinTheirIndices) {
    NullDevice device;
    const Device::Buffer vertices = device.makeBuffer({.size = 64, .usage = Device::Usage::Dynamic, .label = "vertices"});
    const Device::Buffer indices = device.makeBuffer({.size = 12, .usage = Device::Usage::Dynamic, .index = true, .label = "indices"});
    device.writeBuffer(vertices, 0, pixels);
    device.writeBuffer(vertices, 16, pixels);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.writeBuffer(vertices, 8, pixels); }), gpu::ContractError::Rule::BufferWrite);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.writeBuffer(vertices, 34, std::span<const std::byte>(pixels).first(4)); }), gpu::ContractError::Rule::BufferWrite);

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
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.draw(1, 5, 1); }), gpu::ContractError::Rule::Draw);
    device.endPass();
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.writeBuffer(vertices, 32, pixels); }), gpu::ContractError::Rule::BufferWrite);
    device.commit();
    EXPECT_NO_THROW(device.writeBuffer(vertices, 0, pixels));
}

// A debug group pushed inside a pass closes in that pass, one pushed between passes closes between them, and a frame commits with none open.
TEST_F(NullDeviceTest, BalancesDebugGroupsWithinTheirPasses) {
    NullDevice device;
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.popDebugGroup(); }), gpu::ContractError::Rule::DebugGroup);
    device.pushDebugGroup("frame");
    device.beginPass({});
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.popDebugGroup(); }), gpu::ContractError::Rule::DebugGroup);
    device.pushDebugGroup("sprites");
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.endPass(); }), gpu::ContractError::Rule::DebugGroup);
    device.popDebugGroup();
    device.endPass();
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.commit(); }), gpu::ContractError::Rule::DebugGroup);
    device.popDebugGroup();
    EXPECT_NO_THROW(device.commit());
}

// The surface goes and comes back in turn between frames, a minimized window resizes it to no pixels, and a window shows its drawable at its size or larger, as when the app caps its pixel density.
TEST_F(NullDeviceTest, TakesTheSurfaceInTurnBetweenFrames) {
    NullDevice device;
    const Surface::Size eight{.width = 8, .height = 8, .windowWidth = 8, .windowHeight = 8};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.attachSurface({.size = eight}); }), gpu::ContractError::Rule::Surface);
    device.detachSurface();
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.detachSurface(); }), gpu::ContractError::Rule::Surface);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.attachSurface({.size = eight, .sampleCount = 3}); }), gpu::ContractError::Rule::Surface);
    device.attachSurface({.size = eight});
    EXPECT_EQ(device.getSurfaceSize(), eight);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.resizeSurface({}); }), gpu::ContractError::Rule::Surface);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.resizeSurface({.width = -1, .height = 4, .windowWidth = 4, .windowHeight = 4}); }), gpu::ContractError::Rule::Surface);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.resizeSurface({.width = 8, .height = 8, .windowWidth = 4, .windowHeight = 8}); }), gpu::ContractError::Rule::Surface);

    const Surface::Size scaled{.width = 4, .height = 4, .windowWidth = 8, .windowHeight = 8};
    device.resizeSurface(scaled);
    EXPECT_EQ(device.getSurfaceSize(), scaled);
    EXPECT_TRUE(device.getSurfaceSize().isScaled());
    device.beginPass({});
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.resizeSurface(eight); }), gpu::ContractError::Rule::PassOrder);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.detachSurface(); }), gpu::ContractError::Rule::PassOrder);
    device.endPass();
    device.commit();
}

// A device may present a surface once a pass into another surface leaves it, so the passes of a frame into one surface follow one another, with offscreen passes between them.
TEST_F(NullDeviceTest, KeepsThePassesOfAFrameIntoOneSurfaceTogether) {
    NullDevice device;
    const int board = device.addSurface({.size = {.width = 8, .height = 8, .windowWidth = 8, .windowHeight = 8}});
    const Device::Image target = device.makeImage({.width = 4, .height = 4, .kind = Device::ImageKind::RenderTarget, .label = "target"});
    const std::array<Device::ColorAttachment, 1> attachments{{{.image = target}}};
    device.beginPass({});
    device.endPass();
    device.beginPass({.colors = attachments});
    device.endPass();
    device.beginPass({.load = Device::Load::Keep});
    device.endPass();
    device.beginPass({.surface = board});
    device.endPass();
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.beginPass({.load = Device::Load::Keep}); }), gpu::ContractError::Rule::Surface);
    device.commit();

    device.beginPass({.surface = board});
    device.endPass();
    device.beginPass({});
    device.endPass();
    device.commit();
}

// A debug group between two swapchain passes ends the run of passes that a multisampled window keeps its samples in, as an offscreen pass does, while a group inside a pass does not.
TEST_F(NullDeviceTest, RefusesToKeepTheColorOfAMultisampledWindowAcrossADebugGroup) {
    NullDevice device;
    device.detachSurface();
    device.attachSurface({.size = {.width = 8, .height = 8, .windowWidth = 8, .windowHeight = 8}, .sampleCount = 4});

    device.beginPass({});
    device.pushDebugGroup("inside");
    device.popDebugGroup();
    device.endPass();
    EXPECT_NO_THROW(device.beginPass({.load = Device::Load::Keep}));
    device.endPass();
    device.pushDebugGroup("between");
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.beginPass({.load = Device::Load::Keep}); }), gpu::ContractError::Rule::Surface);
    device.popDebugGroup();
    EXPECT_NO_THROW(device.beginPass({.load = Device::Load::Clear}));
    device.endPass();
    device.commit();
}

// A multisampled window keeps its color only while its swapchain passes follow one another, so a pass that keeps it after an offscreen pass would draw over lost samples, while a window of one sample keeps its color across one.
TEST_F(NullDeviceTest, RefusesToKeepTheColorOfAMultisampledWindowAcrossAnOffscreenPass) {
    NullDevice device;
    const Device::Image target = device.makeImage({.width = 4, .height = 4, .kind = Device::ImageKind::RenderTarget, .label = "target"});
    const std::array<Device::ColorAttachment, 1> attachments{{{.image = target}}};
    device.detachSurface();
    device.attachSurface({.size = {.width = 8, .height = 8, .windowWidth = 8, .windowHeight = 8}, .sampleCount = 4});

    device.beginPass({.colors = attachments});
    device.endPass();
    device.beginPass({});
    device.endPass();
    device.beginPass({.load = Device::Load::Keep});
    device.endPass();
    device.beginPass({.colors = attachments});
    device.endPass();
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.beginPass({.load = Device::Load::Keep}); }), gpu::ContractError::Rule::Surface);
    EXPECT_NO_THROW(device.beginPass({.load = Device::Load::Clear}));
    device.endPass();
    device.commit();

    device.detachSurface();
    device.attachSurface({.size = {.width = 8, .height = 8, .windowWidth = 8, .windowHeight = 8}});
    device.beginPass({});
    device.endPass();
    device.beginPass({.colors = attachments});
    device.endPass();
    EXPECT_NO_THROW(device.beginPass({.load = Device::Load::Keep}));
    device.endPass();
    device.commit();
}

// A read copies a whole render target of one sample, between passes, and hands its pixels over once.
TEST_F(NullDeviceTest, ReadsWholeRenderTargetsOfOneSample) {
    NullDevice device;
    const Device::Image target = device.makeImage({.width = 4, .height = 2, .kind = Device::ImageKind::RenderTarget, .label = "target"});
    const Device::Image samples = device.makeImage({.width = 4, .height = 2, .sampleCount = 4, .kind = Device::ImageKind::RenderTarget, .label = "samples"});
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.readImage(samples, 4, 2); }), gpu::ContractError::Rule::Readback);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device.readImage(target, 2, 2); }), gpu::ContractError::Rule::Readback);
    const std::unique_ptr<Device::Readback> copy = device.readImage(target, 4, 2);
    EXPECT_FALSE(copy->poll().has_value());
    const std::optional<std::vector<std::uint8_t>> read = copy->poll();
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->size(), 4U * 2U * 4U);
    EXPECT_FALSE(copy->poll().has_value());
}

} // namespace haylen::gpu
