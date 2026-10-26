#include "gpu/vulkan/UploadRing.hpp"

#include <algorithm>

#include "gpu/vulkan/Context.hpp"

namespace haylen::gpu::vulkan {

UploadRing::UploadRing(const Context& owner, VkBufferUsageFlags bufferUsage, VkDeviceSize defaultChunkSize, int frameCount, std::string_view name) : context(owner), usage(bufferUsage), chunkSize(defaultChunkSize), label(name), frames(static_cast<std::size_t>(frameCount)) {}

UploadRing::~UploadRing() {
    for (const std::vector<Chunk>& chunks : frames) {
        for (const Chunk& made : chunks) {
            destroyChunk(made);
        }
    }
}

void UploadRing::begin(int frame) {
    current = static_cast<std::size_t>(frame);
    chunk = 0;
    std::vector<Chunk>& chunks = frames[current];
    // clang-format off
    std::erase_if(chunks, [this](const Chunk& made) {
        if (made.size > chunkSize) {
            destroyChunk(made);
            return true;
        }
        return false;
    });
    // clang-format on
    for (Chunk& made : chunks) {
        made.cursor = 0;
    }
}

UploadRing::Allocation UploadRing::allocate(VkDeviceSize size, VkDeviceSize alignment) {
    std::vector<Chunk>& chunks = frames[current];
    for (; chunk < chunks.size(); ++chunk) {
        Chunk& made = chunks[chunk];
        const VkDeviceSize offset = (made.cursor + alignment - 1) / alignment * alignment;
        if (offset + size <= made.size) {
            made.cursor = offset + size;
            return {.buffer = made.buffer, .offset = offset, .data = made.data + offset};
        }
    }

    chunks.push_back(makeChunk(std::max(size, chunkSize)));
    chunk = chunks.size() - 1;
    Chunk& made = chunks.back();
    made.cursor = size;
    return {.buffer = made.buffer, .offset = 0, .data = made.data};
}

void UploadRing::flush() {
    for (const Chunk& made : frames[current]) {
        if (made.cursor > 0) {
            vmaFlushAllocation(context.getAllocator(), made.allocation, 0, made.cursor);
        }
    }
}

UploadRing::Chunk UploadRing::makeChunk(VkDeviceSize size) const {
    const VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    const VmaAllocationCreateInfo memory{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
    Chunk made{.size = size};
    VmaAllocationInfo allocated{};
    Context::check(vmaCreateBuffer(context.getAllocator(), &info, &memory, &made.buffer, &made.allocation, &allocated), "make an upload buffer");
    made.data = static_cast<std::byte*>(allocated.pMappedData);
    context.setName(VK_OBJECT_TYPE_BUFFER, made.buffer, label);
    return made;
}

void UploadRing::destroyChunk(const Chunk& made) const {
    vmaDestroyBuffer(context.getAllocator(), made.buffer, made.allocation);
}

} // namespace haylen::gpu::vulkan
