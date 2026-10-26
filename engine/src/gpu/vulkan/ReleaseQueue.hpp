#pragma once

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include <volk.h>

#include <vk_mem_alloc.h>

namespace haylen::gpu::vulkan {

class Context;

// The objects of Vulkan that wait for the frames that may still use them to finish on the GPU before they are destroyed. Each object waits for the frame of its serial, and the serials of the objects never go down.
class ReleaseQueue final {
  public:
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    struct Image {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    struct ImageView {
        VkImageView view = VK_NULL_HANDLE;
    };

    struct Sampler {
        VkSampler sampler = VK_NULL_HANDLE;
    };

    struct ShaderModule {
        VkShaderModule module = VK_NULL_HANDLE;
    };

    struct Pipeline {
        VkPipeline pipeline = VK_NULL_HANDLE;
    };

    struct Framebuffer {
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
    };

    struct DescriptorSet {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };

    using Object = std::variant<Buffer, Image, ImageView, Sampler, ShaderModule, Pipeline, Framebuffer, DescriptorSet>;

    explicit ReleaseQueue(const Context& owner) noexcept : context(owner) {}
    ~ReleaseQueue();
    ReleaseQueue(const ReleaseQueue&) = delete;
    ReleaseQueue& operator=(const ReleaseQueue&) = delete;

    void push(Object object, std::uint64_t serial);

    // Destroys the objects whose frames finished, the ones up to the serial.
    void collect(std::uint64_t completed);

    // Destroys every object, once the GPU finished all of its work.
    void clear();

  private:
    void destroy(const Object& object) const;

    const Context& context;
    std::vector<std::pair<std::uint64_t, Object>> pending;
};

} // namespace haylen::gpu::vulkan
