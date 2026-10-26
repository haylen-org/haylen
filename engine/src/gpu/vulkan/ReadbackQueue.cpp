#include "gpu/vulkan/ReadbackQueue.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "gpu/vulkan/Context.hpp"
#include "gpu/vulkan/Conversions.hpp"
#include "gpu/vulkan/ImageLayouts.hpp"

namespace haylen::gpu::vulkan {

ReadbackQueue::ReadbackQueue(const Context& owner) : context(owner) {
    const VkCommandPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, .queueFamilyIndex = context.getQueueFamily()};
    Context::check(context.getFunctions().vkCreateCommandPool(context.getDevice(), &info, nullptr, &pool), "make the command pool of its readbacks");
}

ReadbackQueue::~ReadbackQueue() {
    for (const std::shared_ptr<Copy>& copy : copies) {
        if (copy->fence != VK_NULL_HANDLE) {
            context.getFunctions().vkWaitForFences(context.getDevice(), 1, &copy->fence, VK_TRUE, UINT64_MAX);
            update(*copy);
        }
        release(*copy);
    }
    context.getFunctions().vkDestroyCommandPool(context.getDevice(), pool, nullptr);
}

std::shared_ptr<ReadbackQueue::Copy> ReadbackQueue::start(const Source& source) {
    if (Conversions::isDepth(source.format)) {
        throw std::invalid_argument("The Vulkan device reads back color images only.");
    }
    auto copy = std::make_shared<Copy>(Copy{.queue = this, .width = source.width, .height = source.height, .format = source.format});
    const VolkDeviceTable& vk = context.getFunctions();
    const VkDevice device = context.getDevice();

    const VkDeviceSize size = static_cast<VkDeviceSize>(source.width) * static_cast<VkDeviceSize>(source.height) * Conversions::getBytesPerPixel(source.format);
    const VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    const VmaAllocationCreateInfo memory{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
    VmaAllocationInfo allocated{};
    Context::check(vmaCreateBuffer(context.getAllocator(), &bufferInfo, &memory, &copy->buffer, &copy->allocation, &allocated), "make the buffer of a readback");
    copy->data = static_cast<const std::byte*>(allocated.pMappedData);

    const VkCommandBufferAllocateInfo commandsInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    try {
        Context::check(vk.vkAllocateCommandBuffers(device, &commandsInfo, &copy->commands), "record a readback");
        Context::check(vk.vkCreateFence(device, &fenceInfo, nullptr, &copy->fence), "make the fence of a readback");
        record(copy->commands, source, copy->buffer);
        const VkSubmitInfo submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &copy->commands};
        Context::check(vk.vkQueueSubmit(context.getQueue(), 1, &submit, copy->fence), "submit a readback");
    } catch (...) {
        release(*copy);
        throw;
    }
    copies.push_back(copy);
    return copy;
}

void ReadbackQueue::record(VkCommandBuffer commands, const Source& source, VkBuffer buffer) const {
    const VolkDeviceTable& vk = context.getFunctions();
    const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    Context::check(vk.vkBeginCommandBuffer(commands, &begin), "record a readback");

    const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
    const VkImageMemoryBarrier toSource = ImageLayouts::makeBarrier(source.image, range, source.layout, source.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vk.vkCmdPipelineBarrier(commands, ImageLayouts::getStages(source.layout), VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);

    const VkBufferImageCopy region{.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1}, .imageExtent = {static_cast<std::uint32_t>(source.width), static_cast<std::uint32_t>(source.height), 1}};
    vk.vkCmdCopyImageToBuffer(commands, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

    const VkImageMemoryBarrier toRest = ImageLayouts::makeBarrier(source.image, range, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, source.layout);
    const VkBufferMemoryBarrier toHost{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .buffer = buffer, .size = VK_WHOLE_SIZE};
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, ImageLayouts::getStages(source.layout) | VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &toHost, 1, &toRest);
    Context::check(vk.vkEndCommandBuffer(commands), "record a readback");
}

void ReadbackQueue::update(Copy& copy) {
    if (copy.fence == VK_NULL_HANDLE) {
        return;
    }
    const VkResult status = context.getFunctions().vkGetFenceStatus(context.getDevice(), copy.fence);
    if (status == VK_NOT_READY) {
        return;
    }
    if (status != VK_SUCCESS) {
        copy.failure = "The Vulkan device was lost before the copy of the render target finished.";
        release(copy);
        return;
    }
    finish(copy);
}

void ReadbackQueue::finish(Copy& copy) {
    vmaInvalidateAllocation(context.getAllocator(), copy.allocation, 0, VK_WHOLE_SIZE);
    copy.pixels = convert(copy.data, copy.width, copy.height, copy.format);
    release(copy);
}

// A copy that its readback no longer holds goes without converting its pixels.
void ReadbackQueue::sweep() {
    // clang-format off
    std::erase_if(copies, [this](const std::shared_ptr<Copy>& copy) {
        if (copy.use_count() == 1 && copy->fence != VK_NULL_HANDLE && context.getFunctions().vkGetFenceStatus(context.getDevice(), copy->fence) != VK_NOT_READY) {
            release(*copy);
        }
        return copy->fence == VK_NULL_HANDLE;
    });
    // clang-format on
}

void ReadbackQueue::release(Copy& copy) const {
    const VolkDeviceTable& vk = context.getFunctions();
    if (copy.fence != VK_NULL_HANDLE) {
        vk.vkDestroyFence(context.getDevice(), copy.fence, nullptr);
        copy.fence = VK_NULL_HANDLE;
    }
    if (copy.commands != VK_NULL_HANDLE) {
        vk.vkFreeCommandBuffers(context.getDevice(), pool, 1, &copy.commands);
        copy.commands = VK_NULL_HANDLE;
    }
    if (copy.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(context.getAllocator(), copy.buffer, copy.allocation);
        copy.buffer = VK_NULL_HANDLE;
        copy.allocation = VK_NULL_HANDLE;
        copy.data = nullptr;
    }
    copy.queue = nullptr;
}

// Every format becomes red, green, blue and alpha in eight bits each, with what a format lacks as 0 for colors and 1 for alpha, and the floats clamped to the range of a byte.
std::vector<std::uint8_t> ReadbackQueue::convert(const std::byte* data, int width, int height, Device::PixelFormat format) {
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> pixels(count * 4);
    if (format == Device::PixelFormat::Rgba8 || format == Device::PixelFormat::Rgba8Srgb) {
        std::memcpy(pixels.data(), data, pixels.size());
        return pixels;
    }

    const std::size_t stride = Conversions::getBytesPerPixel(format);
    for (std::size_t index = 0; index < count; ++index) {
        const std::byte* source = data + index * stride;
        std::array<float, 4> channels{0.0F, 0.0F, 0.0F, 1.0F};
        std::array<std::uint8_t, 4> bytes{};
        std::memcpy(bytes.data(), source, std::min<std::size_t>(stride, 4));
        std::array<std::uint16_t, 4> halves{};
        std::array<float, 4> floats{};
        std::memcpy(halves.data(), source, std::min<std::size_t>(stride, 8));
        std::memcpy(floats.data(), source, std::min<std::size_t>(stride, 16));

        switch (format) {
        case Device::PixelFormat::R8:
            channels[0] = bytes[0] / 255.0F;
            break;
        case Device::PixelFormat::Rg8:
            channels[0] = bytes[0] / 255.0F;
            channels[1] = bytes[1] / 255.0F;
            break;
        case Device::PixelFormat::Bgra8:
            channels = {bytes[2] / 255.0F, bytes[1] / 255.0F, bytes[0] / 255.0F, bytes[3] / 255.0F};
            break;
        case Device::PixelFormat::Rgb10A2: {
            const std::uint32_t packed = std::bit_cast<std::uint32_t>(bytes);
            channels = {static_cast<float>(packed & 0x3FFU) / 1023.0F, static_cast<float>((packed >> 10) & 0x3FFU) / 1023.0F, static_cast<float>((packed >> 20) & 0x3FFU) / 1023.0F, static_cast<float>(packed >> 30) / 3.0F};
            break;
        }
        case Device::PixelFormat::R16Float:
        case Device::PixelFormat::Rg16Float:
        case Device::PixelFormat::Rgba16Float:
            for (std::size_t channel = 0; channel < stride / 2; ++channel) {
                channels[channel] = toFloat(halves[channel]);
            }
            break;
        case Device::PixelFormat::R32Float:
        case Device::PixelFormat::Rg32Float:
        case Device::PixelFormat::Rgba32Float:
            for (std::size_t channel = 0; channel < stride / 4; ++channel) {
                channels[channel] = floats[channel];
            }
            break;
        default:
            break;
        }
        for (std::size_t channel = 0; channel < 4; ++channel) {
            pixels[index * 4 + channel] = toByte(channels[channel]);
        }
    }
    return pixels;
}

float ReadbackQueue::toFloat(std::uint16_t half) noexcept {
    const std::uint32_t sign = (half & 0x8000U) << 16;
    const std::uint32_t exponent = (half >> 10) & 0x1FU;
    const std::uint32_t mantissa = half & 0x3FFU;
    if (exponent == 0) {
        const float value = std::ldexp(static_cast<float>(mantissa), -24);
        return sign != 0 ? -value : value;
    }
    if (exponent == 31) {
        return std::bit_cast<float>(sign | 0x7F800000U | (mantissa << 13));
    }
    return std::bit_cast<float>(sign | ((exponent + 112) << 23) | (mantissa << 13));
}

std::uint8_t ReadbackQueue::toByte(float value) noexcept {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

} // namespace haylen::gpu::vulkan
