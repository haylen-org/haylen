#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <volk.h>

#include <vk_mem_alloc.h>

namespace haylen::gpu::vulkan {

class Context;

// Memory that the CPU writes and the GPU reads within one frame, in buffers that stay mapped. Each frame in flight takes from chunks of its own, which it reuses once the GPU finished the frame, so a frame that writes what the frame before it wrote makes no buffer. A write larger than a chunk takes a chunk of its size, which goes once its frame comes around again.
class UploadRing final {
  public:
    struct Allocation {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        std::byte* data = nullptr;
    };

    UploadRing(const Context& owner, VkBufferUsageFlags bufferUsage, VkDeviceSize defaultChunkSize, int frameCount, std::string_view name);
    ~UploadRing();
    UploadRing(const UploadRing&) = delete;
    UploadRing& operator=(const UploadRing&) = delete;

    // Starts taking from the chunks of a frame in flight, whose earlier use the GPU finished.
    void begin(int frame);

    [[nodiscard]] Allocation allocate(VkDeviceSize size, VkDeviceSize alignment);

    // Makes what the frame wrote visible to the GPU, on memory that the CPU does not keep coherent.
    void flush();

  private:
    struct Chunk {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        std::byte* data = nullptr;
        VkDeviceSize size = 0;
        VkDeviceSize cursor = 0;
    };

    [[nodiscard]] Chunk makeChunk(VkDeviceSize size) const;
    void destroyChunk(const Chunk& chunk) const;

    const Context& context;
    VkBufferUsageFlags usage = 0;
    VkDeviceSize chunkSize = 0;
    std::string label;
    std::vector<std::vector<Chunk>> frames;
    std::size_t current = 0;
    std::size_t chunk = 0;
};

} // namespace haylen::gpu::vulkan
