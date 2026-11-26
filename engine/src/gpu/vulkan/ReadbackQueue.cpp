#include "gpu/vulkan/ReadbackQueue.hpp"

#include <algorithm>
#include <cstring>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "gpu/ImageInfo.hpp"
#include "gpu/PixelConverter.hpp"
#include "gpu/vulkan/Context.hpp"
#include "gpu/vulkan/Conversions.hpp"
#include "gpu/vulkan/ImageLayouts.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu::vulkan {

ReadbackQueue::ReadbackQueue(const Context& owner) : context(owner) {
    const VkCommandPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = nullptr, .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, .queueFamilyIndex = context.getQueueFamily()};
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

std::shared_ptr<ReadbackQueue::Copy> ReadbackQueue::start(const ImageSource& source) {
    if (ImageInfo::isDepth(source.format)) {
        throw ContractError(ContractError::Rule::Readback);
    }

    const auto size = static_cast<VkDeviceSize>(ImageInfo::getRegionBytes(source.format, source.width, source.height));
    std::shared_ptr<Copy> copy = open(size);
    copy->pixels = Pixels{.width = source.width, .height = source.height, .format = source.format};
    recordImage(copy->commands, source, copy->buffer);
    submit(copy);
    return copy;
}

std::shared_ptr<ReadbackQueue::Copy> ReadbackQueue::start(const BufferSource& source) {
    std::shared_ptr<Copy> copy = open(source.size);
    recordBuffer(copy->commands, source, copy->buffer);
    submit(copy);
    return copy;
}

// A copy starts with a buffer of its size that the CPU reads and a command buffer that records it.
std::shared_ptr<ReadbackQueue::Copy> ReadbackQueue::open(VkDeviceSize size) {
    auto copy = std::make_shared<Copy>(Copy{.queue = this, .size = size});
    const VolkDeviceTable& vk = context.getFunctions();
    const VkDevice device = context.getDevice();

    const VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .pNext = nullptr, .flags = 0, .size = size, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .queueFamilyIndexCount = 0, .pQueueFamilyIndices = nullptr};
    const VmaAllocationCreateInfo memory{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO, .requiredFlags = 0, .preferredFlags = 0, .memoryTypeBits = 0, .pool = VK_NULL_HANDLE, .pUserData = nullptr, .priority = 0.0F, .minAlignment = 0};
    VmaAllocationInfo allocated{};
    Context::check(vmaCreateBuffer(context.getAllocator(), &bufferInfo, &memory, &copy->buffer, &copy->allocation, &allocated), "make the buffer of a readback");
    copy->data = static_cast<const std::byte*>(allocated.pMappedData);

    const VkCommandBufferAllocateInfo commandsInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .pNext = nullptr, .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0};
    const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = nullptr, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, .pInheritanceInfo = nullptr};
    try {
        Context::check(vk.vkAllocateCommandBuffers(device, &commandsInfo, &copy->commands), "record a readback");
        Context::check(vk.vkCreateFence(device, &fenceInfo, nullptr, &copy->fence), "make the fence of a readback");
        Context::check(vk.vkBeginCommandBuffer(copy->commands, &begin), "record a readback");
    } catch (...) {
        release(*copy);
        throw;
    }

    return copy;
}

void ReadbackQueue::submit(const std::shared_ptr<Copy>& copy) {
    const VolkDeviceTable& vk = context.getFunctions();
    try {
        Context::check(vk.vkEndCommandBuffer(copy->commands), "record a readback");
        const VkSubmitInfo info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = nullptr, .waitSemaphoreCount = 0, .pWaitSemaphores = nullptr, .pWaitDstStageMask = nullptr, .commandBufferCount = 1, .pCommandBuffers = &copy->commands, .signalSemaphoreCount = 0, .pSignalSemaphores = nullptr};
        Context::check(vk.vkQueueSubmit(context.getQueue(), 1, &info, copy->fence), "submit a readback");
    } catch (...) {
        release(*copy);
        throw;
    }

    copies.push_back(copy);
}

void ReadbackQueue::recordImage(VkCommandBuffer commands, const ImageSource& source, VkBuffer buffer) const {
    const VolkDeviceTable& vk = context.getFunctions();
    const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};
    const VkImageMemoryBarrier toSource = ImageLayouts::makeBarrier(source.image, range, source.layout, source.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vk.vkCmdPipelineBarrier(commands, ImageLayouts::getStages(source.layout), VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);

    const VkBufferImageCopy region{.bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0, .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1}, .imageOffset = {}, .imageExtent = {static_cast<std::uint32_t>(source.width), static_cast<std::uint32_t>(source.height), 1}};
    vk.vkCmdCopyImageToBuffer(commands, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

    const VkImageMemoryBarrier toRest = ImageLayouts::makeBarrier(source.image, range, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, source.layout);
    const VkBufferMemoryBarrier toHost = makeHostBarrier(buffer);
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, ImageLayouts::getStages(source.layout) | VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &toHost, 1, &toRest);
}

// The frames make what they wrote into a buffer visible to the copies that follow them in the queue, so the copy reads the range at once.
void ReadbackQueue::recordBuffer(VkCommandBuffer commands, const BufferSource& source, VkBuffer buffer) const {
    const VolkDeviceTable& vk = context.getFunctions();
    const VkBufferCopy region{.srcOffset = source.offset, .dstOffset = 0, .size = source.size};
    vk.vkCmdCopyBuffer(commands, source.buffer, buffer, 1, &region);

    const VkBufferMemoryBarrier toHost = makeHostBarrier(buffer);
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &toHost, 0, nullptr);
}

VkBufferMemoryBarrier ReadbackQueue::makeHostBarrier(VkBuffer buffer) noexcept {
    return {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .pNext = nullptr, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .buffer = buffer, .offset = 0, .size = VK_WHOLE_SIZE};
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
        copy.failure = copy.pixels ? "The Vulkan device was lost before the copy of the render target finished." : "The Vulkan device was lost before the copy of the buffer finished.";
        release(copy);
        return;
    }

    finish(copy);
}

void ReadbackQueue::finish(Copy& copy) {
    vmaInvalidateAllocation(context.getAllocator(), copy.allocation, 0, VK_WHOLE_SIZE);
    if (copy.pixels) {
        copy.bytes = convert(copy.data, *copy.pixels);
    } else {
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(copy.size));
        std::memcpy(bytes.data(), copy.data, bytes.size());
        copy.bytes = std::move(bytes);
    }

    release(copy);
}

// A copy that its readback no longer holds goes without taking its bytes.
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

bool ReadbackQueue::isCopying() const {
    const VolkDeviceTable& vk = context.getFunctions();
    return std::ranges::any_of(copies, [&](const std::shared_ptr<Copy>& copy) { return copy->fence != VK_NULL_HANDLE && vk.vkGetFenceStatus(context.getDevice(), copy->fence) == VK_NOT_READY; });
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

std::vector<std::uint8_t> ReadbackQueue::convert(const std::byte* data, const Pixels& pixels) {
    const std::size_t rowBytes = ImageInfo::getRowBytes(pixels.format, pixels.width);
    const std::size_t pixelRow = static_cast<std::size_t>(pixels.width) * 4;
    std::vector<std::uint8_t> converted(pixelRow * static_cast<std::size_t>(pixels.height));
    for (std::size_t row = 0; row < static_cast<std::size_t>(pixels.height); ++row) {
        PixelConverter::convertRow(pixels.format, {data + row * rowBytes, rowBytes}, std::span(converted).subspan(row * pixelRow, pixelRow));
    }

    return converted;
}

} // namespace haylen::gpu::vulkan
