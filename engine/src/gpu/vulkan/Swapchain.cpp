#include "gpu/vulkan/Swapchain.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

#include "gpu/vulkan/Context.hpp"
#include "gpu/vulkan/Conversions.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::gpu::vulkan {

void Swapchain::attach(const Context& owner) {
    if (!owner.hasSwapchain() || !owner.canPresent(surface)) {
        throw std::runtime_error("The GPU of the Vulkan device cannot present to the window.");
    }
    context = &owner;
    depthFormat = owner.getDepthStencilFormat();

    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->getPhysicalDevice(), surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->getPhysicalDevice(), surface, &count, formats.data());
    const VkSurfaceFormatKHR chosen = chooseFormat(count, formats.data());
    format = chosen.format;
    colorSpace = chosen.colorSpace;
}

Swapchain::~Swapchain() {
    if (context != nullptr) {
        destroyImages();
    }
    if (handle != VK_NULL_HANDLE) {
        context->getFunctions().vkDestroySwapchainKHR(context->getDevice(), handle, nullptr);
    }
    vkDestroySurfaceKHR(instance, surface, nullptr);
}

// The engine blends in the values it stores, so the swapchain takes eight bits a channel without the conversion of sRGB formats.
VkSurfaceFormatKHR Swapchain::chooseFormat(std::uint32_t count, const VkSurfaceFormatKHR* formats) noexcept {
    for (const VkFormat preferred : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}) {
        for (std::uint32_t index = 0; index < count; ++index) {
            if (formats[index].format == preferred && formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return formats[index];
            }
        }
    }
    if (count == 0 || formats[0].format == VK_FORMAT_UNDEFINED) {
        return {.format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }
    return formats[0];
}

// Without vsync the frames replace the waiting one without tearing where the surface allows it, and reach the screen at once otherwise.
VkPresentModeKHR Swapchain::choosePresentMode(bool vsync) const {
    if (vsync) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->getPhysicalDevice(), surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->getPhysicalDevice(), surface, &count, modes.data());
    for (const VkPresentModeKHR preferred : {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR}) {
        if (std::ranges::find(modes, preferred) != modes.end()) {
            return preferred;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkCompositeAlphaFlagBitsKHR Swapchain::chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported, bool transparent) noexcept {
    const std::array<VkCompositeAlphaFlagBitsKHR, 4> transparentOrder{VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR};
    const std::array<VkCompositeAlphaFlagBitsKHR, 4> opaqueOrder{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR};
    for (const VkCompositeAlphaFlagBitsKHR mode : transparent ? transparentOrder : opaqueOrder) {
        if ((supported & static_cast<VkCompositeAlphaFlagsKHR>(mode)) != 0) {
            return mode;
        }
    }
    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

// A surface that leaves the size to the swapchain takes the size of the drawable, and a surface that the compositor turns, as on Android, reports its size before the turn.
VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkSurfaceTransformFlagBitsKHR transform, const Settings& settings) noexcept {
    VkExtent2D chosen = capabilities.currentExtent;
    if (chosen.width == 0xFFFFFFFFU) {
        chosen = {static_cast<std::uint32_t>(std::max(settings.width, 0)), static_cast<std::uint32_t>(std::max(settings.height, 0))};
    } else if (transform == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR && (capabilities.currentTransform & (VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR | VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)) != 0) {
        std::swap(chosen.width, chosen.height);
    }
    if (chosen.width == 0 || chosen.height == 0) {
        return {};
    }
    chosen.width = std::clamp(chosen.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    chosen.height = std::clamp(chosen.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return chosen;
}

bool Swapchain::configure(const Settings& settings) {
    const VkPhysicalDevice physicalDevice = context->getPhysicalDevice();
    VkSurfaceCapabilitiesKHR capabilities{};
    Context::check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities), "read the surface of the window");

    // The compositor turns the images when the surface allows it, so the frames draw as the window shows them.
    const VkSurfaceTransformFlagBitsKHR transform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0 ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
    const VkExtent2D chosen = chooseExtent(capabilities, transform, settings);
    destroyImages();
    if (chosen.width == 0) {
        if (handle != VK_NULL_HANDLE) {
            context->getFunctions().vkDestroySwapchainKHR(context->getDevice(), handle, nullptr);
            handle = VK_NULL_HANDLE;
        }
        extent = {};
        return false;
    }
    extent = chosen;

    const VkSampleCountFlags counts = context->getProperties().limits.framebufferColorSampleCounts & context->getProperties().limits.framebufferDepthSampleCounts;
    samples = VK_SAMPLE_COUNT_1_BIT;
    for (int count = settings.sampleCount; count > 1; count /= 2) {
        if ((counts & static_cast<VkSampleCountFlags>(Conversions::toSampleCount(count))) != 0) {
            samples = Conversions::toSampleCount(count);
            break;
        }
    }

    const VkCompositeAlphaFlagBitsKHR alpha = chooseCompositeAlpha(capabilities.supportedCompositeAlpha, settings.transparent);
    if (settings.transparent && alpha != VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR && alpha != VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR && !warnedOpaque) {
        core::Log::warning("The window asks for transparency, which its Vulkan surface does not composite, so it shows opaque.");
        warnedOpaque = true;
    }

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }
    const VkSwapchainKHR old = handle;
    const VkSwapchainCreateInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = format,
        .imageColorSpace = colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = transform,
        .compositeAlpha = alpha,
        .presentMode = choosePresentMode(settings.vsync),
        .clipped = VK_TRUE,
        .oldSwapchain = old,
    };
    const VolkDeviceTable& vk = context->getFunctions();
    const VkResult result = vk.vkCreateSwapchainKHR(context->getDevice(), &info, nullptr, &handle);
    if (old != VK_NULL_HANDLE) {
        vk.vkDestroySwapchainKHR(context->getDevice(), old, nullptr);
    }
    if (result != VK_SUCCESS) {
        handle = VK_NULL_HANDLE;
        Context::check(result, "make the swapchain of the window");
    }

    std::uint32_t count = 0;
    vk.vkGetSwapchainImagesKHR(context->getDevice(), handle, &count, nullptr);
    std::vector<VkImage> made(count);
    vk.vkGetSwapchainImagesKHR(context->getDevice(), handle, &count, made.data());
    images.resize(count);
    const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (std::uint32_t index = 0; index < count; ++index) {
        images[index].image = made[index];
        images[index].view = makeView(made[index], format);
        Context::check(vk.vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &images[index].finished), "make a semaphore");
        context->setName(VK_OBJECT_TYPE_IMAGE, made[index], "haylen-swapchain-image");
    }

    if (samples != VK_SAMPLE_COUNT_1_BIT) {
        color = makeAttachment(format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        context->setName(VK_OBJECT_TYPE_IMAGE, color.image, "haylen-swapchain-color");
    }
    depth = makeAttachment(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    context->setName(VK_OBJECT_TYPE_IMAGE, depth.image, "haylen-swapchain-depth");
    return true;
}

VkResult Swapchain::acquire(VkSemaphore signaled, std::uint32_t& index) const {
    return context->getFunctions().vkAcquireNextImageKHR(context->getDevice(), handle, UINT64_MAX, signaled, VK_NULL_HANDLE, &index);
}

VkResult Swapchain::present(std::uint32_t index) const {
    const VkPresentInfoKHR info{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1, .pWaitSemaphores = &images[index].finished, .swapchainCount = 1, .pSwapchains = &handle, .pImageIndices = &index};
    return context->getFunctions().vkQueuePresentKHR(context->getQueue(), &info);
}

std::vector<VkImageView> Swapchain::getViews() const {
    std::vector<VkImageView> views;
    for (const SwapchainImage& image : images) {
        views.push_back(image.view);
    }
    for (const VkImageView view : {color.view, depth.view}) {
        if (view != VK_NULL_HANDLE) {
            views.push_back(view);
        }
    }
    return views;
}

VkImageView Swapchain::makeView(VkImage image, VkFormat viewFormat) const {
    const VkImageViewCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = viewFormat, .subresourceRange = {.aspectMask = Conversions::getAspects(viewFormat), .levelCount = 1, .layerCount = 1}};
    VkImageView view = VK_NULL_HANDLE;
    Context::check(context->getFunctions().vkCreateImageView(context->getDevice(), &info, nullptr, &view), "make a view of the swapchain");
    return view;
}

// GPUs without memory that is allocated lazily keep the attachment in ordinary memory of the GPU.
Swapchain::Attachment Swapchain::makeAttachment(VkFormat attachmentFormat, VkImageUsageFlags usage) const {
    const VkImageCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = attachmentFormat,
        .extent = {extent.width, extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    Attachment made;
    VmaAllocationCreateInfo memory{.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED};
    if (vmaCreateImage(context->getAllocator(), &info, &memory, &made.image, &made.allocation, nullptr) != VK_SUCCESS) {
        memory.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        Context::check(vmaCreateImage(context->getAllocator(), &info, &memory, &made.image, &made.allocation, nullptr), "make an attachment of the swapchain");
    }
    made.view = makeView(made.image, attachmentFormat);
    return made;
}

void Swapchain::destroyAttachment(Attachment& attachment) const {
    if (attachment.image == VK_NULL_HANDLE) {
        return;
    }
    context->getFunctions().vkDestroyImageView(context->getDevice(), attachment.view, nullptr);
    vmaDestroyImage(context->getAllocator(), attachment.image, attachment.allocation);
    attachment = {};
}

void Swapchain::destroyImages() {
    const VolkDeviceTable& vk = context->getFunctions();
    for (const SwapchainImage& image : images) {
        vk.vkDestroyImageView(context->getDevice(), image.view, nullptr);
        vk.vkDestroySemaphore(context->getDevice(), image.finished, nullptr);
    }
    images.clear();
    destroyAttachment(color);
    destroyAttachment(depth);
}

} // namespace haylen::gpu::vulkan
