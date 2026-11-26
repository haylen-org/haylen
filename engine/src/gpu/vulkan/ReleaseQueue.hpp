#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include <volk.h>

#include <vk_mem_alloc.h>

namespace haylen::gpu::vulkan {

class Context;

// The objects of Vulkan that wait for the frames that may still use them to finish on the GPU before they are destroyed. Each object waits at least for the frame of its serial.
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

    // The room in the queue of one object that the device makes, which the object keeps once its owner holds it and gives back otherwise, so an object that fails to be made leaves the queue as it was.
    class Hold final {
      public:
        explicit Hold(ReleaseQueue& owner);
        ~Hold();
        Hold(const Hold&) = delete;
        Hold& operator=(const Hold&) = delete;

        void keep() noexcept {
            queue = nullptr;
        }

      private:
        ReleaseQueue* queue;
    };

    explicit ReleaseQueue(const Context& owner) noexcept : context(owner) {}
    ~ReleaseQueue();
    ReleaseQueue(const ReleaseQueue&) = delete;
    ReleaseQueue& operator=(const ReleaseQueue&) = delete;

    // Holds the room of an object before the device makes it, since every object that the queue destroys later has its place in the queue from the moment it is made, so `push` never allocates.
    [[nodiscard]] Hold hold() {
        return Hold(*this);
    }

    // Gives back the room of an object that leaves without the queue, such as a set that the cache of descriptors frees itself.
    void forgo() noexcept;

    void push(Object object, std::uint64_t serial) noexcept;

    // Destroys the objects whose frames finished, the ones up to the serial.
    void collect(std::uint64_t completed);

    // Destroys every object, once the GPU finished all of its work.
    void clear();

  private:
    void destroy(const Object& object) const;

    const Context& context;
    std::vector<std::pair<std::uint64_t, Object>> pending;
    std::size_t held = 0;
};

} // namespace haylen::gpu::vulkan
