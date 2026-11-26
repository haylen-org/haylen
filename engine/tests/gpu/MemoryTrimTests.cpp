#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "gpu/support/GpuFixture.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::gpu {

// A device gives back the memory that a burst of uploads left behind once it is trimmed, and a frame that draws what the frame before it drew allocates nothing after the trim.
class GpuMemoryTrimTest : public test::GpuFixture {
  protected:
    static constexpr int kSize = 8;
    static constexpr int kBurstSide = 1024;
    static constexpr int kBurstImages = 8;
    // More frames than any device keeps in flight, so every frame in flight finished and started again.
    static constexpr int kSettleFrames = 8;
    // Draws whose uniforms of at least 32 bytes each take more than the 1 MB of a chunk of the uniform memory of a device.
    static constexpr int kBurstDraws = 1 << 16;

    void SetUp() override {
        GpuFixture::SetUp();

        if (!IsSkipped() && !HasFatalFailure()) {
            target = makeTarget(kSize, kSize);
            const std::array<Device::ColorTarget, 1> formats{};
            pipeline = makePlanePipeline(formats);
        }
    }

    void drawFrame() {
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = math::Color::black()}}};
        device().beginPass({.colors = colors, .label = "haylen-test-trim"});
        device().applyPipeline(pipeline);
        drawPlane(math::Color::white());
        device().endPass();
        device().commit();
    }

    void drawFrames(int count) {
        for (int frame = 0; frame < count; ++frame) {
            drawFrame();
        }
    }

    // Uploads the pixels of large dynamic images in one frame, as a burst of loads does, and destroys them.
    void uploadBurst() {
        const std::vector<std::byte> pixels(static_cast<std::size_t>(kBurstSide) * kBurstSide * 4, std::byte{128});
        std::vector<Device::Image> burst;
        std::vector<Device::ImageWrite> writes;
        for (int index = 0; index < kBurstImages; ++index) {
            burst.push_back(device().makeImage({.width = kBurstSide, .height = kBurstSide, .kind = Device::ImageKind::Dynamic, .label = "haylen-test-burst"}));
            writes.push_back({.image = burst.back(), .region = {.width = kBurstSide, .height = kBurstSide}, .pixels = pixels});
        }

        device().writeImages(writes);
        drawFrame();
        for (const Device::Image image : burst) {
            device().destroy(image);
        }

        drawFrames(kSettleFrames);
    }

    // Draws the plane many times in one frame, so its uniforms take more uniform memory than the steady frames need.
    void drawUniformBurst() {
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = math::Color::black()}}};
        device().beginPass({.colors = colors, .label = "haylen-test-uniform-burst"});
        device().applyPipeline(pipeline);
        for (int draw = 0; draw < kBurstDraws; ++draw) {
            drawPlane(math::Color::white());
        }

        device().endPass();
        device().commit();
    }

    [[nodiscard]] std::vector<Device::Pool> readPools() {
        std::vector<Device::Pool> pools;
        device().readPools(pools);
        return pools;
    }

    Device::Image target{};
    Device::Pipeline pipeline{};
};

TEST_P(GpuMemoryTrimTest, GivesBackWhatABurstOfUploadsLeftBehind) {
    drawFrames(kSettleFrames);
    device().trim();
    drawFrames(kSettleFrames);
    const std::size_t before = device().getSpareBytes();

    uploadBurst();
    device().trim();
    drawFrames(kSettleFrames);
    EXPECT_LE(device().getSpareBytes(), before);

    const test::AllocationTracker tracker;
    drawFrames(120);
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_LE(device().getSpareBytes(), before);
    EXPECT_TRUE(isNear(read(target, kSize, kSize).at(4, 4), math::Color::white()));
}

// The uniform memory that a trim gives back goes with everything that names it, such as the descriptor sets of Vulkan, which would otherwise name a buffer whose handle a later buffer may take, so every pool of the device holds what it held before the burst.
TEST_P(GpuMemoryTrimTest, LetsGoOfWhatNamesTheUniformMemoryItGivesBack) {
    drawFrames(kSettleFrames);
    device().trim();
    drawFrames(kSettleFrames);
    const std::vector<Device::Pool> before = readPools();

    drawUniformBurst();
    drawFrames(kSettleFrames);
    device().trim();
    drawFrames(kSettleFrames);

    const std::vector<Device::Pool> after = readPools();
    ASSERT_EQ(after.size(), before.size());
    for (std::size_t index = 0; index < after.size(); ++index) {
        EXPECT_EQ(after[index].used, before[index].used) << after[index].name;
    }

    EXPECT_TRUE(isNear(read(target, kSize, kSize).at(4, 4), math::Color::white()));
}

// A trim between frames, after the frames in flight finished, gives back at once the memory of the objects destroyed since the last frame, as a trim in the background does.
TEST_P(GpuMemoryTrimTest, GivesBackAtOnceBetweenFinishedFrames) {
    drawFrames(kSettleFrames);
    device().trim();
    drawFrames(kSettleFrames);
    (void)read(target, kSize, kSize);
    const std::size_t before = device().getSpareBytes();

    const std::vector<std::byte> pixels(static_cast<std::size_t>(kBurstSide) * kBurstSide * 4, std::byte{64});
    const std::array<std::span<const std::byte>, 1> levels{pixels};
    const Device::Image image = device().makeImage({.width = kBurstSide, .height = kBurstSide, .levels = levels, .label = "haylen-test-released"});
    drawFrame();
    (void)read(target, kSize, kSize);
    device().destroy(image);

    device().trim();
    EXPECT_LE(device().getSpareBytes(), before);
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuMemoryTrimTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuMemoryTrimTest);

} // namespace haylen::gpu
