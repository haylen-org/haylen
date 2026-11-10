#include "gpu/vulkan/Swapchain.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

#include "gpu/vulkan/Context.hpp"
#include "gpu/vulkan/Conversions.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::gpu::vulkan {

void Swapchain::attach(const Context& owner, std::size_t frames) {
    if (!owner.hasSwapchain() || !owner.canPresent(surface)) {
        throw std::runtime_error("The queue of the Vulkan device cannot present to the surface.");
    }
    context = &owner;
    depthFormat = owner.getDepthStencilFormat();
    const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    acquired.resize(frames, VK_NULL_HANDLE);
    for (VkSemaphore& semaphore : acquired) {
        Context::check(owner.getFunctions().vkCreateSemaphore(owner.getDevice(), &semaphoreInfo, nullptr, &semaphore), "make a semaphore of the swapchain");
    }

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
        for (const VkSemaphore semaphore : acquired) {
            context->getFunctions().vkDestroySemaphore(context->getDevice(), semaphore, nullptr);
        }
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

// A surface that leaves the size to the swapchain takes the size of the window. The images keep the size of the window as it shows, since the compositor turns them where the display turns.
VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Settings& settings) noexcept {
    VkExtent2D chosen = capabilities.currentExtent;
    if (chosen.width == 0xFFFFFFFFU) {
        chosen = {static_cast<std::uint32_t>(std::max(settings.size.windowWidth, 0)), static_cast<std::uint32_t>(std::max(settings.size.windowHeight, 0))};
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
    // A surface that the window system took back, such as the window of a backgrounded Android app, gives no swapchain until the platform hands over a new one.
    VkSurfaceCapabilitiesKHR capabilities{};
    const VkResult described = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
    if (described == VK_ERROR_SURFACE_LOST_KHR) {
        destroyImages();
        destroySwapchain();
        return false;
    }
    Context::check(described, "read the surface of the window");

    // The compositor turns the images when the surface allows it, so the frames draw as the window shows them.
    const VkSurfaceTransformFlagBitsKHR transform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0 ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
    const VkExtent2D chosen = chooseExtent(capabilities, settings);
    destroyImages();
    if (chosen.width == 0) {
        destroySwapchain();
        return false;
    }
    presentedExtent = chosen;
    extent = chosen;

    // A window that shows a smaller drawable, whose surface fixes the size of the images, takes the drawable through a copy into each image.
    const VkExtent2D wanted{static_cast<std::uint32_t>(std::max(settings.size.width, 1)), static_cast<std::uint32_t>(std::max(settings.size.height, 1))};
    const bool scaled = settings.size.isScaled() && (wanted.width != chosen.width || wanted.height != chosen.height);
    if (scaled && !canScale(capabilities)) {
        throw std::runtime_error("The Vulkan surface of the window takes no copies into its images, so this GPU cannot draw the window with fewer pixels than it shows. Remove the limit of the pixel density, or draw with another graphics API.");
    }
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (scaled) {
        extent = {std::min(wanted.width, chosen.width), std::min(wanted.height, chosen.height)};
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

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
        .imageExtent = presentedExtent,
        .imageArrayLayers = 1,
        .imageUsage = usage,
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
    if (result == VK_ERROR_SURFACE_LOST_KHR || result == VK_ERROR_NATIVE_WINDOW_IN_USE_KHR) {
        handle = VK_NULL_HANDLE;
        extent = {};
        presentedExtent = {};
        return false;
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
    if (scaled) {
        drawables.resize(count);
        for (Attachment& drawable : drawables) {
            drawable = makeDrawable();
        }
    }

    if (samples != VK_SAMPLE_COUNT_1_BIT) {
        color = makeAttachment(format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        context->setName(VK_OBJECT_TYPE_IMAGE, color.image, "haylen-swapchain-color");
    }
    depth = makeAttachment(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    context->setName(VK_OBJECT_TYPE_IMAGE, depth.image, "haylen-swapchain-depth");
    return true;
}

// The surface must take copies into its images, and its format must copy between images with the linear filter, which Vulkan requires of the formats that `chooseFormat` prefers.
bool Swapchain::canScale(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
        return false;
    }

    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(context->getPhysicalDevice(), format, &properties);
    const VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    return (properties.optimalTilingFeatures & needed) == needed;
}

void Swapchain::recordScale(VkCommandBuffer commands, std::uint32_t index) const {
    const VolkDeviceTable& vk = context->getFunctions();
    const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
    const VkImage source = drawables[index].image;
    const VkImage target = images[index].image;

    const std::array<VkImageMemoryBarrier, 2> toCopy{{
        {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT, .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = source, .subresourceRange = range},
        {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = target, .subresourceRange = range},
    }};
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(toCopy.size()), toCopy.data());

    const VkImageSubresourceLayers layers{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1};
    const VkImageBlit region{
        .srcSubresource = layers,
        .srcOffsets = {{0, 0, 0}, {static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height), 1}},
        .dstSubresource = layers,
        .dstOffsets = {{0, 0, 0}, {static_cast<std::int32_t>(presentedExtent.width), static_cast<std::int32_t>(presentedExtent.height), 1}},
    };
    vk.vkCmdBlitImage(commands, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);

    const VkImageMemoryBarrier toPresent{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = 0, .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = target, .subresourceRange = range};
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);
}

VkResult Swapchain::acquire(std::size_t frame, std::uint32_t& index) const {
    return context->getFunctions().vkAcquireNextImageKHR(context->getDevice(), handle, UINT64_MAX, acquired[frame], VK_NULL_HANDLE, &index);
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
    for (const Attachment& drawable : drawables) {
        views.push_back(drawable.view);
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

// The drawable of an image of the swapchain keeps what the passes of a frame drew until the frame copies it into that image.
Swapchain::Attachment Swapchain::makeDrawable() const {
    const VkImageCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {extent.width, extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    Attachment made;
    const VmaAllocationCreateInfo memory{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    Context::check(vmaCreateImage(context->getAllocator(), &info, &memory, &made.image, &made.allocation, nullptr), "make a drawable of the swapchain");
    made.view = makeView(made.image, format);
    context->setName(VK_OBJECT_TYPE_IMAGE, made.image, "haylen-swapchain-drawable");
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

void Swapchain::destroySwapchain() {
    if (handle != VK_NULL_HANDLE) {
        context->getFunctions().vkDestroySwapchainKHR(context->getDevice(), handle, nullptr);
        handle = VK_NULL_HANDLE;
    }
    extent = {};
    presentedExtent = {};
}

void Swapchain::destroyImages() {
    const VolkDeviceTable& vk = context->getFunctions();
    for (const SwapchainImage& image : images) {
        vk.vkDestroyImageView(context->getDevice(), image.view, nullptr);
        vk.vkDestroySemaphore(context->getDevice(), image.finished, nullptr);
    }
    images.clear();
    for (Attachment& drawable : drawables) {
        destroyAttachment(drawable);
    }
    drawables.clear();
    destroyAttachment(color);
    destroyAttachment(depth);
}

} // namespace haylen::gpu::vulkan
