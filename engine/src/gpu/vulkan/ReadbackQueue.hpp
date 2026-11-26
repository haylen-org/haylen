#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <volk.h>

#include <vk_mem_alloc.h>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::vulkan {

class Context;

// The copies of render targets and of ranges of buffers into memory the CPU reads. Each copy goes to the queue in a command buffer of its own with a fence, right after the frames that wrote what it reads, and the CPU takes its bytes once the fence signals, converting the pixels of a render target to four bytes, so no frame waits for it. Copies that nobody polls any more go once they finish, and the copies still running when the device goes finish first.
class ReadbackQueue final {
  public:
    // The size and format of the pixels that the copy of a render target holds, which the CPU converts to four bytes a pixel.
    struct Pixels {
        int width = 0;
        int height = 0;
        Device::PixelFormat format = Device::PixelFormat::Rgba8;
    };

    // A copy as its readback sees it, which outlives the queue: the queue is null once the copy finished or the device went. The copy of a buffer has no pixels and hands its bytes over as they are.
    struct Copy {
        ReadbackQueue* queue = nullptr;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        const std::byte* data = nullptr;
        VkDeviceSize size = 0;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        std::optional<Pixels> pixels{};
        std::optional<std::vector<std::uint8_t>> bytes{};
        std::string failure{};
    };

    // What the copy of a render target reads: the image, the layout it rests in between passes and its size and format.
    struct ImageSource {
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        int width = 0;
        int height = 0;
        Device::PixelFormat format = Device::PixelFormat::Rgba8;
    };

    // What the copy of a buffer reads: a range of the buffer.
    struct BufferSource {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        VkDeviceSize size = 0;
    };

    explicit ReadbackQueue(const Context& owner);
    ~ReadbackQueue();
    ReadbackQueue(const ReadbackQueue&) = delete;
    ReadbackQueue& operator=(const ReadbackQueue&) = delete;

    [[nodiscard]] std::shared_ptr<Copy> start(const ImageSource& source);
    [[nodiscard]] std::shared_ptr<Copy> start(const BufferSource& source);

    // Takes the bytes of the copy once the GPU finished it.
    void update(Copy& copy);

    // Lets go of the finished copies that no readback holds.
    void sweep();

    // Whether a copy still runs on the GPU, which reads an image or a buffer that the device keeps until it finishes.
    [[nodiscard]] bool isCopying() const;

  private:
    [[nodiscard]] std::shared_ptr<Copy> open(VkDeviceSize size);
    void submit(const std::shared_ptr<Copy>& copy);
    void recordImage(VkCommandBuffer commands, const ImageSource& source, VkBuffer buffer) const;
    void recordBuffer(VkCommandBuffer commands, const BufferSource& source, VkBuffer buffer) const;
    [[nodiscard]] static VkBufferMemoryBarrier makeHostBarrier(VkBuffer buffer) noexcept;
    void finish(Copy& copy);
    void release(Copy& copy) const;
    [[nodiscard]] static std::vector<std::uint8_t> convert(const std::byte* data, const Pixels& pixels);

    const Context& context;
    VkCommandPool pool = VK_NULL_HANDLE;
    std::vector<std::shared_ptr<Copy>> copies;
};

} // namespace haylen::gpu::vulkan
