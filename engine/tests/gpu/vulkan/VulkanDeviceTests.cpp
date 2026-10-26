#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "gpu/support/LoggedErrors.hpp"
#include "gpu/vulkan/Device.hpp"

namespace haylen::gpu::vulkan {

// The Vulkan device with dynamic rendering turned off, so the render passes and framebuffers that GPUs without dynamic rendering take run on every machine with Vulkan, under its validation.
class VulkanDeviceTest : public ::testing::Test {
  protected:
    static constexpr int kSize = 16;
    static constexpr std::chrono::seconds kReadTimeout{10};

    void SetUp() override {
        errors = std::make_unique<test::LoggedErrors>();
        try {
            device = std::make_unique<Device>(std::nullopt, Device::Settings{.validation = true, .dynamicRendering = false});
        } catch (const std::runtime_error& error) {
            GTEST_SKIP() << "The Vulkan device cannot start on this machine: " << error.what();
        }
    }

    void TearDown() override {
        device.reset();
        if (!IsSkipped()) {
            errors->reportAsFailures();
        }
    }

    [[nodiscard]] std::vector<std::uint8_t> read(gpu::Device::Image image) {
        const std::unique_ptr<gpu::Device::Readback> copy = device->readImage(image, kSize, kSize);
        const auto deadline = std::chrono::steady_clock::now() + kReadTimeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (std::optional<std::vector<std::uint8_t>> pixels = copy->poll()) {
                return *pixels;
            }
            device->commit();
        }
        ADD_FAILURE() << "The copy of the render target did not finish.";
        return {};
    }

    void clear(gpu::Device::Image image, math::Color color, gpu::Device::Image resolve = {}) {
        const gpu::Device::ColorAttachment attachment{.image = image, .store = resolve.isValid() ? gpu::Device::Store::Discard : gpu::Device::Store::Store, .clear = color, .resolve = resolve};
        device->beginPass({.colors = std::span(&attachment, 1), .label = "haylen-test-clear"});
        device->endPass();
    }

    std::unique_ptr<test::LoggedErrors> errors;
    std::unique_ptr<Device> device;
};

TEST_F(VulkanDeviceTest, ResolvesAndKeepsAttachmentsThroughRenderPasses) {
    ASSERT_FALSE(device->isRenderingDynamically());
    const gpu::Device::Image multisampled = device->makeImage({.width = kSize, .height = kSize, .sampleCount = 4, .kind = gpu::Device::ImageKind::RenderTarget, .label = "haylen-test-multisampled"});
    const gpu::Device::Image target = device->makeImage({.width = kSize, .height = kSize, .kind = gpu::Device::ImageKind::RenderTarget, .label = "haylen-test-target"});

    clear(multisampled, math::Color{0.0F, 1.0F, 0.0F, 1.0F}, target);
    const gpu::Device::ColorAttachment kept{.image = target, .load = gpu::Device::Load::Keep};
    device->beginPass({.colors = std::span(&kept, 1)});
    device->endPass();
    device->commit();

    const std::vector<std::uint8_t> pixels = read(target);
    ASSERT_EQ(pixels.size(), static_cast<std::size_t>(kSize * kSize * 4));
    EXPECT_EQ((std::array<std::uint8_t, 4>{pixels[0], pixels[1], pixels[2], pixels[3]}), (std::array<std::uint8_t, 4>{0, 255, 0, 255}));
    device->destroy(multisampled);
    device->destroy(target);
}

// The framebuffers of destroyed render targets go with them, while the frames in flight may still draw with them.
TEST_F(VulkanDeviceTest, ReleasesTheFramebuffersOfDestroyedImages) {
    const std::vector<gpu::Device::Pool> before = device->getPools();
    for (int frame = 0; frame < 32; ++frame) {
        const gpu::Device::Image target = device->makeImage({.width = kSize, .height = kSize, .kind = gpu::Device::ImageKind::RenderTarget});
        clear(target, math::Color{1.0F, 0.0F, 0.0F, 1.0F});
        device->commit();
        device->destroy(target);
    }
    device->commit();

    const std::vector<gpu::Device::Pool> after = device->getPools();
    ASSERT_EQ(after.size(), before.size());
    for (std::size_t index = 0; index < after.size(); ++index) {
        EXPECT_EQ(after[index].used, before[index].used) << after[index].name;
    }
}

TEST_F(VulkanDeviceTest, DrawsNothingIntoTheSwapchainOfADeviceWithoutASurface) {
    device->beginPass({.load = gpu::Device::Load::Clear, .label = "haylen-test-swapchain"});
    device->applyViewport({0.0F, 0.0F, 8.0F, 8.0F});
    device->draw(0, 3, 1);
    device->endPass();
    device->commit();
    EXPECT_FALSE(device->isLost());
}

} // namespace haylen::gpu::vulkan
