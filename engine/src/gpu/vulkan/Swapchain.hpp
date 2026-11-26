#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <volk.h>

#include <vk_mem_alloc.h>

#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::vulkan {

class Context;

// The swapchain of the surface of a window, with the multisampled color image that resolves into its images when the window draws with several samples and the depth and stencil image of its passes. Those two images last while swapchain passes that keep them follow one another, and no pass stores them, so they live in memory that tile GPUs never allocate. A drawable smaller than the images that the surface takes, as when the app caps its pixel density on a window whose surface fixes their size, draws into an image of its own for each image of the swapchain, which the frame scales into the image of the swapchain before it presents it.
class Swapchain final {
  public:
    static constexpr std::uint64_t kAcquireTimeout = 1'000'000'000;

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

    // Acquires the next image for the frame in flight, whose semaphore `getAcquired` gives to the submission that waits for it. It waits at most `timeout` nanoseconds and gives `VK_TIMEOUT` when the window system kept every image that long.
    [[nodiscard]] VkResult acquire(std::size_t frame, std::uint64_t timeout, std::uint32_t& index) const;
    // Presents the image at the index, with the id of the present where the device waits for presents, which `waitForDisplay` then waits for.
    [[nodiscard]] VkResult present(std::uint32_t index);

    // Waits at most `timeout` nanoseconds until the display shows the image that the swapchain presented last, where the device waits for presents, and gives `VK_TIMEOUT` when the display took longer. A swapchain that presented nothing since the last wait waits for nothing.
    [[nodiscard]] VkResult waitForDisplay(std::uint64_t timeout);

    // Notes the moment the next frame reads its input, on the steady clock, which the device calls as the wait for the display ends.
    void markInput() noexcept;

    // Reads when the display showed the presents that it showed since the last read, where the swapchain reports it, which gives the age of the input of the last of them.
    void readTimings();

    [[nodiscard]] std::optional<double> getInputAge() const noexcept {
        return inputAge;
    }

    [[nodiscard]] VkSemaphore getAcquired(std::size_t frame) const noexcept {
        return acquired[frame];
    }

    // The present mode of the frames, which is FIFO with vertical sync and where the surface offers no other.
    [[nodiscard]] static VkPresentModeKHR choosePresentMode(std::span<const VkPresentModeKHR> modes, bool vsync) noexcept;

    // The present mode that the swapchain presents with since it was last configured.
    [[nodiscard]] VkPresentModeKHR getPresentMode() const noexcept {
        return presentMode;
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

    // The size that the passes draw, which is the size of the drawable when the swapchain scales it, turned with the images.
    [[nodiscard]] VkExtent2D getExtent() const noexcept {
        return extent;
    }

    // The quarter turns of the images from the window, as `SurfaceTurn` describes.
    [[nodiscard]] int getTurns() const noexcept {
        return turns;
    }

    // The size that the passes draw as the window shows it, before the turn of the images.
    [[nodiscard]] VkExtent2D getWindowExtent() const noexcept {
        return turnExtent(extent, turns);
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

    [[nodiscard]] static VkSurfaceFormatKHR chooseFormat(std::span<const VkSurfaceFormatKHR> formats) noexcept;
    [[nodiscard]] std::vector<VkPresentModeKHR> readPresentModes() const;
    [[nodiscard]] static VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported, bool transparent) noexcept;
    [[nodiscard]] static VkSurfaceTransformFlagBitsKHR chooseTransform(const VkSurfaceCapabilitiesKHR& capabilities) noexcept;
    [[nodiscard]] static int toTurns(VkSurfaceTransformFlagBitsKHR transform) noexcept;
    [[nodiscard]] static VkExtent2D turnExtent(VkExtent2D size, int quarterTurns) noexcept;
    [[nodiscard]] static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Settings& settings, int quarterTurns) noexcept;
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
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    // A present that the display timing follows, with the nanoseconds of the steady clock when its frame read its input.
    struct TimedPresent {
        std::uint32_t id = 0;
        std::uint64_t inputNanoseconds = 0;
    };

    // The presents that the display timing follows at once, more than the images of any swapchain.
    static constexpr std::size_t kTimedPresents = 8;

    // The id of the last present, which grows with every present of the surface, and the id that the next wait waits for, or 0 when no present waits.
    std::uint64_t presentId = 0;
    std::uint64_t pendingPresent = 0;

    // The presents that the display timing follows, the moment the next frame reads its input and the age of the input of the last present that the display showed.
    std::array<TimedPresent, kTimedPresents> timedPresents{};
    std::array<VkPastPresentationTimingGOOGLE, kTimedPresents> timings{};
    std::uint32_t timingId = 0;
    std::uint64_t inputNanoseconds = 0;
    std::optional<double> inputAge;
    int turns = 0;
    bool warnedOpaque = false;
    bool warnedVsync = false;
};

} // namespace haylen::gpu::vulkan
