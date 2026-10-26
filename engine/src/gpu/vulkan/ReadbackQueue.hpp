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

// The copies of render targets into memory the CPU reads. Each copy goes to the queue in a command buffer of its own with a fence, right after the frames that drew the image, and the CPU converts its pixels to four bytes once the fence signals, so no frame waits for it. Copies that nobody polls any more go once they finish, and the copies still running when the device goes finish first.
class ReadbackQueue final {
  public:
    // A copy as its readback sees it, which outlives the queue: the queue is null once the device went.
    struct Copy {
        ReadbackQueue* queue = nullptr;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        const std::byte* data = nullptr;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        int width = 0;
        int height = 0;
        Device::PixelFormat format = Device::PixelFormat::Rgba8;
        std::optional<std::vector<std::uint8_t>> pixels;
        std::string failure;
    };

    // What the copy reads: the image, the layout it rests in between passes and its size and format.
    struct Source {
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        int width = 0;
        int height = 0;
        Device::PixelFormat format = Device::PixelFormat::Rgba8;
    };

    explicit ReadbackQueue(const Context& owner);
    ~ReadbackQueue();
    ReadbackQueue(const ReadbackQueue&) = delete;
    ReadbackQueue& operator=(const ReadbackQueue&) = delete;

    [[nodiscard]] std::shared_ptr<Copy> start(const Source& source);

    // Takes the pixels of the copy once the GPU finished it.
    void update(Copy& copy);

    // Lets go of the finished copies that no readback holds.
    void sweep();

  private:
    void record(VkCommandBuffer commands, const Source& source, VkBuffer buffer) const;
    void finish(Copy& copy);
    void release(Copy& copy) const;
    [[nodiscard]] static std::vector<std::uint8_t> convert(const std::byte* data, int width, int height, Device::PixelFormat format);
    [[nodiscard]] static float toFloat(std::uint16_t half) noexcept;
    [[nodiscard]] static std::uint8_t toByte(float value) noexcept;

    const Context& context;
    VkCommandPool pool = VK_NULL_HANDLE;
    std::vector<std::shared_ptr<Copy>> copies;
};

} // namespace haylen::gpu::vulkan
