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
#include "gpu/vulkan/Swapchain.hpp"

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

        throw std::runtime_error("The copy of the render target did not finish.");
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
    std::vector<gpu::Device::Pool> before;
    device->readPools(before);

    for (int frame = 0; frame < 32; ++frame) {
        const gpu::Device::Image target = device->makeImage({.width = kSize, .height = kSize, .kind = gpu::Device::ImageKind::RenderTarget});
        clear(target, math::Color{1.0F, 0.0F, 0.0F, 1.0F});
        device->commit();
        device->destroy(target);
    }

    device->commit();

    std::vector<gpu::Device::Pool> after;
    device->readPools(after);
    ASSERT_EQ(after.size(), before.size());

    for (std::size_t index = 0; index < after.size(); ++index) {
        EXPECT_EQ(after[index].used, before[index].used) << after[index].name;
    }
}

// Vulkan would make such an image against its rules, which only validation reports.
TEST_F(VulkanDeviceTest, RefusesImagesBeyondTheLimitsOfTheirFormat) {
    EXPECT_THROW((void)device->makeImage({.width = kSize, .height = kSize, .mipLevels = 6, .kind = gpu::Device::ImageKind::RenderTarget, .label = "haylen-test-levels"}), std::invalid_argument);
    EXPECT_THROW((void)device->makeImage({.width = kSize, .height = kSize, .layers = 2, .kind = gpu::Device::ImageKind::RenderTarget, .label = "haylen-test-flat-layers"}), std::invalid_argument);
    EXPECT_THROW((void)device->makeImage({.type = gpu::Device::TextureType::Array, .width = kSize, .height = kSize, .layers = 1 << 24, .label = "haylen-test-layers"}), std::invalid_argument);
}

TEST_F(VulkanDeviceTest, DrawsNothingIntoTheSwapchainOfADeviceWithoutASurface) {
    device->beginPass({.load = gpu::Device::Load::Clear, .label = "haylen-test-swapchain"});
    device->applyViewport({0.0F, 0.0F, 8.0F, 8.0F});
    device->draw(0, 3, 1);
    device->endPass();
    device->commit();
    EXPECT_FALSE(device->isLost());
}

// FIFO is the one mode that every surface offers, so a surface without the others presents the frames at the pace of the display, which the swapchain reports.
TEST(VulkanSwapchainTest, PresentsWithoutVerticalSyncOnlyWhereTheSurfaceOffersIt) {
    const std::array<VkPresentModeKHR, 3> every{VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR};
    const std::array<VkPresentModeKHR, 2> immediate{VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR};
    const std::array<VkPresentModeKHR, 1> fifo{VK_PRESENT_MODE_FIFO_KHR};

    EXPECT_EQ(Swapchain::choosePresentMode(every, true), VK_PRESENT_MODE_FIFO_KHR);
    EXPECT_EQ(Swapchain::choosePresentMode(every, false), VK_PRESENT_MODE_MAILBOX_KHR);
    EXPECT_EQ(Swapchain::choosePresentMode(immediate, false), VK_PRESENT_MODE_IMMEDIATE_KHR);
    EXPECT_EQ(Swapchain::choosePresentMode(fifo, false), VK_PRESENT_MODE_FIFO_KHR);
}

} // namespace haylen::gpu::vulkan
