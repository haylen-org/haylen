#include <gtest/gtest.h>

#include <array>
#include <cstddef>
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
TEST_F(NullDeviceTest, RunsOutOfRoomLikeTheDeviceOfTheSokolBackend) {
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

// The colors of a pipeline share one blend and one write mask, which OpenGL applies to every color of a pass.
TEST_F(NullDeviceTest, RejectsPipelinesWhoseColorsBlendApart) {
    NullDevice device;
    const Device::Program program = device.makeProgram({.label = "empty"});
    const Device::Blend adding{.enabled = true, .sourceColor = Device::BlendFactor::SourceAlpha, .destinationColor = Device::BlendFactor::One};
    const std::array<Device::ColorTarget, 2> alike{{{.blend = adding}, {.blend = adding}}};
    EXPECT_NO_THROW((void)device.makePipeline({.program = program, .colors = alike, .label = "alike"}));
    const std::array<Device::ColorTarget, 2> blends{{{.blend = adding}, {}}};
    EXPECT_THROW((void)device.makePipeline({.program = program, .colors = blends, .label = "blends"}), std::logic_error);
    const std::array<Device::ColorTarget, 2> masks{{{.blend = adding}, {.writeMask = Device::ColorMask::None, .blend = adding}}};
    EXPECT_THROW((void)device.makePipeline({.program = program, .colors = masks, .label = "masks"}), std::logic_error);
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

} // namespace haylen::gpu
