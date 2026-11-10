#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <volk.h>

#include <vk_mem_alloc.h>

#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::vulkan {

class Context;

// The swapchain of the surface of a window, with the multisampled color image that resolves into its images when the window draws with several samples and the depth and stencil image of its passes. Those two images last for one pass, so they live in memory that tile GPUs never allocate. A drawable smaller than the images that the surface takes, as when the app caps its pixel density on a window whose surface fixes their size, draws into an image of its own for each image of the swapchain, which the frame scales into the image of the swapchain before it presents it.
class Swapchain final {
  public:
    struct Settings {
        Surface::Size size;
        int sampleCount = 1;
        bool vsync = true;
        bool transparent = false;
    };

    // Takes the surface of the instance, which it destroys with itself.
    Swapchain(VkInstance owner, VkSurfaceKHR made) noexcept : instance(owner), surface(made) {}
    ~Swapchain();
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Joins the device that draws into the surface, whose queue must present to it, with a semaphore of acquired images for each of its frames in flight. Throws `std::runtime_error` when the queue cannot present to the surface.
    void attach(const Context& owner, std::size_t frames);

    [[nodiscard]] VkSurfaceKHR getSurface() const noexcept {
        return surface;
    }

    // Makes the swapchain again for the settings once the GPU finished every frame, and gives whether it has images, which a window without pixels lacks.
    bool configure(const Settings& settings);

    // Acquires the next image for the frame in flight, whose semaphore `getAcquired` gives to the submission that waits for it.
    [[nodiscard]] VkResult acquire(std::size_t frame, std::uint32_t& index) const;
    [[nodiscard]] VkResult present(std::uint32_t index) const;

    [[nodiscard]] VkSemaphore getAcquired(std::size_t frame) const noexcept {
        return acquired[frame];
    }

    [[nodiscard]] bool hasImages() const noexcept {
        return handle != VK_NULL_HANDLE;
    }

    [[nodiscard]] VkFormat getFormat() const noexcept {
        return format;
    }

    [[nodiscard]] VkFormat getDepthFormat() const noexcept {
        return depthFormat;
    }

    [[nodiscard]] VkSampleCountFlagBits getSamples() const noexcept {
        return samples;
    }

    // The size that the passes draw, which is the size of the drawable when the swapchain scales it.
    [[nodiscard]] VkExtent2D getExtent() const noexcept {
        return extent;
    }

    // Whether the passes draw into images of their own, which `recordScale` scales into the images of the swapchain.
    [[nodiscard]] bool isScaled() const noexcept {
        return !drawables.empty();
    }

    // The image that the passes draw into for the image of the swapchain at the index, and its view.
    [[nodiscard]] VkImage getImage(std::uint32_t index) const noexcept {
        return isScaled() ? drawables[index].image : images[index].image;
    }

    [[nodiscard]] VkImageView getView(std::uint32_t index) const noexcept {
        return isScaled() ? drawables[index].view : images[index].view;
    }

    // Records the scale of the drawable that the passes drew for the image at the index into that image of the swapchain, with the linear filter, and leaves the image ready to present.
    void recordScale(VkCommandBuffer commands, std::uint32_t index) const;

    // The semaphore that the frame drawing into the image signals for its presentation.
    [[nodiscard]] VkSemaphore getFinished(std::uint32_t index) const noexcept {
        return images[index].finished;
    }

    [[nodiscard]] VkImage getColorImage() const noexcept {
        return color.image;
    }

    [[nodiscard]] VkImageView getColorView() const noexcept {
        return color.view;
    }

    [[nodiscard]] VkImage getDepthImage() const noexcept {
        return depth.image;
    }

    [[nodiscard]] VkImageView getDepthView() const noexcept {
        return depth.view;
    }

    // The views of the images of the swapchain and of its attachments, which framebuffers hold.
    [[nodiscard]] std::vector<VkImageView> getViews() const;

  private:
    struct SwapchainImage {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSemaphore finished = VK_NULL_HANDLE;
    };

    struct Attachment {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };

    [[nodiscard]] static VkSurfaceFormatKHR chooseFormat(std::uint32_t count, const VkSurfaceFormatKHR* formats) noexcept;
    [[nodiscard]] VkPresentModeKHR choosePresentMode(bool vsync) const;
    [[nodiscard]] static VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported, bool transparent) noexcept;
    [[nodiscard]] static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Settings& settings) noexcept;
    [[nodiscard]] bool canScale(const VkSurfaceCapabilitiesKHR& capabilities) const;
    [[nodiscard]] VkImageView makeView(VkImage image, VkFormat viewFormat) const;
    [[nodiscard]] Attachment makeAttachment(VkFormat attachmentFormat, VkImageUsageFlags usage) const;
    [[nodiscard]] Attachment makeDrawable() const;
    void destroyAttachment(Attachment& attachment) const;
    void destroyImages();
    void destroySwapchain();

    VkInstance instance = VK_NULL_HANDLE;
    const Context* context = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkExtent2D extent{};
    VkExtent2D presentedExtent{};
    std::vector<SwapchainImage> images;
    std::vector<Attachment> drawables;
    std::vector<VkSemaphore> acquired;
    Attachment color;
    Attachment depth;
    bool warnedOpaque = false;
};

} // namespace haylen::gpu::vulkan
