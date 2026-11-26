#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <volk.h>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::vulkan {

class Context;
class ReleaseQueue;

// The descriptor sets of a device, which outlive the frames that use them. A set of textures and samplers is made once for each layout and each choice of images and samplers, so a frame that binds what the frame before it bound writes no descriptor, and it goes when one of its images or samplers is destroyed or, past a bound, when no frame used it for a while. A set of uniform blocks points each block at its buffer, a buffer of the uniform ring or a uniform buffer of the bindings, with the size of the block, since programs of one layout may hold blocks of other sizes, and each draw passes the offsets of its blocks as dynamic offsets. It goes once one of its buffers goes.
class DescriptorCache final {
  public:
    // How many sets of textures and samplers stay once frames no longer use them.
    static constexpr std::size_t kMaxResourceSets = 4096;

    // The ids of the images and samplers in the slots that a layout reads, with 0 in the others.
    struct ResourceKey {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        std::array<std::uint64_t, Device::kMaxTextures> images{};
        std::array<std::uint64_t, Device::kMaxSamplers> samplers{};

        friend bool operator==(const ResourceKey&, const ResourceKey&) = default;
    };

    struct UniformBlock {
        std::uint32_t binding = 0;
        VkDeviceSize size = 0;

        friend bool operator==(const UniformBlock&, const UniformBlock&) = default;
    };

    DescriptorCache(const Context& owner, ReleaseQueue& releases) noexcept : context(owner), releaseQueue(releases) {}
    ~DescriptorCache();
    DescriptorCache(const DescriptorCache&) = delete;
    DescriptorCache& operator=(const DescriptorCache&) = delete;

    // The set of the key that the frame of the serial uses, or null when the cache has none.
    [[nodiscard]] VkDescriptorSet findResourceSet(const ResourceKey& key, std::uint64_t serial);

    // Makes the set of the key with the writes, whose destination set it fills in.
    [[nodiscard]] VkDescriptorSet addResourceSet(const ResourceKey& key, std::span<VkWriteDescriptorSet> writes, std::uint64_t serial);

    // The set that points each block at the buffer of the same index.
    [[nodiscard]] VkDescriptorSet getUniformSet(VkDescriptorSetLayout layout, std::span<const UniformBlock> blocks, std::span<const VkBuffer> buffers);

    // Lets go of the sets that hold a destroyed image or sampler once the frames that may use them finished.
    void forgetImage(std::uint64_t id, std::uint64_t serial);
    void forgetSampler(std::uint64_t id, std::uint64_t serial);

    // Lets go of the sets of uniform blocks that point at a buffer that goes, once the frames up to the serial finished, and at once for the lookups, so a buffer that takes its handle later never finds them.
    void forgetUniformBuffer(VkBuffer buffer, std::uint64_t serial) noexcept;

    // Frees the sets that no unfinished frame uses while the cache holds more than its bound.
    void trim(std::uint64_t completed);

    // The sets that the cache holds and the sets that its pools hold room for.
    [[nodiscard]] int getSetCount() const noexcept {
        return static_cast<int>(resourceSets.size() + uniformSets.size());
    }

    [[nodiscard]] int getSetCapacity() const noexcept {
        return static_cast<int>(pools.size() * kSetsPerPool);
    }

  private:
    static constexpr std::uint32_t kSetsPerPool = 256;

    struct Entry {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
        std::uint64_t used = 0;
    };

    struct KeyHash {
        [[nodiscard]] std::size_t operator()(const ResourceKey& key) const noexcept;
    };

    struct UniformKey {
        std::uint64_t layout = 0;
        std::array<std::uint64_t, Device::kMaxBlocks> buffers{};
        std::array<VkDeviceSize, Device::kMaxBlocks> ranges{};

        friend auto operator<=>(const UniformKey&, const UniformKey&) = default;
    };

    [[nodiscard]] std::pair<VkDescriptorPool, VkDescriptorSet> allocate(VkDescriptorSetLayout layout);
    [[nodiscard]] VkDescriptorPool makePool() const;
    template <typename Predicate> void forget(Predicate&& holds, std::uint64_t serial);

    const Context& context;
    ReleaseQueue& releaseQueue;
    std::vector<VkDescriptorPool> pools;
    std::unordered_map<ResourceKey, Entry, KeyHash> resourceSets;
    std::map<UniformKey, Entry> uniformSets;
};

} // namespace haylen::gpu::vulkan
