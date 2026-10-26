#include "gpu/vulkan/DescriptorCache.hpp"

#include <algorithm>
#include <array>

#include "gpu/vulkan/Context.hpp"
#include "gpu/vulkan/ReleaseQueue.hpp"

namespace haylen::gpu::vulkan {

DescriptorCache::~DescriptorCache() {
    for (VkDescriptorPool pool : pools) {
        context.getFunctions().vkDestroyDescriptorPool(context.getDevice(), pool, nullptr);
    }
}

std::size_t DescriptorCache::KeyHash::operator()(const ResourceKey& key) const noexcept {
    std::uint64_t hash = 14695981039346656037ULL ^ Context::toHandle(key.layout);
    // clang-format off
    const auto mix = [&hash](std::uint32_t value) {
        hash = (hash ^ value) * 1099511628211ULL;
    };
    // clang-format on
    for (std::uint32_t image : key.images) {
        mix(image);
    }
    for (std::uint32_t sampler : key.samplers) {
        mix(sampler);
    }
    return static_cast<std::size_t>(hash ^ (hash >> 32));
}

VkDescriptorSet DescriptorCache::findResourceSet(const ResourceKey& key, std::uint64_t serial) {
    const auto found = resourceSets.find(key);
    if (found == resourceSets.end()) {
        return VK_NULL_HANDLE;
    }
    found->second.used = serial;
    return found->second.set;
}

VkDescriptorSet DescriptorCache::addResourceSet(const ResourceKey& key, std::span<VkWriteDescriptorSet> writes, std::uint64_t serial) {
    const auto [pool, set] = allocate(key.layout);
    for (VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }
    context.getFunctions().vkUpdateDescriptorSets(context.getDevice(), static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    resourceSets.emplace(key, Entry{.pool = pool, .set = set, .used = serial});
    return set;
}

VkDescriptorSet DescriptorCache::getUniformSet(VkDescriptorSetLayout layout, VkBuffer buffer, std::span<const UniformBlock> blocks) {
    UniformKey key{.layout = Context::toHandle(layout), .buffer = Context::toHandle(buffer)};
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        key.ranges[index] = blocks[index].size;
    }
    if (const auto found = uniformSets.find(key); found != uniformSets.end()) {
        return found->second;
    }

    const VkDescriptorSet set = allocate(layout).second;
    std::array<VkDescriptorBufferInfo, Device::kMaxBlocks> infos{};
    std::array<VkWriteDescriptorSet, Device::kMaxBlocks> writes{};
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        infos[index] = {.buffer = buffer, .offset = 0, .range = blocks[index].size};
        writes[index] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = blocks[index].binding, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .pBufferInfo = &infos[index]};
    }
    context.getFunctions().vkUpdateDescriptorSets(context.getDevice(), static_cast<std::uint32_t>(blocks.size()), writes.data(), 0, nullptr);
    uniformSets.emplace(key, set);
    return set;
}

template <typename Predicate> void DescriptorCache::forget(Predicate&& holds, std::uint64_t serial) {
    for (auto entry = resourceSets.begin(); entry != resourceSets.end();) {
        if (!holds(entry->first)) {
            ++entry;
            continue;
        }
        releaseQueue.push(ReleaseQueue::DescriptorSet{.pool = entry->second.pool, .set = entry->second.set}, serial);
        entry = resourceSets.erase(entry);
    }
}

void DescriptorCache::forgetImage(std::uint32_t id, std::uint64_t serial) {
    forget([id](const ResourceKey& key) { return std::ranges::find(key.images, id) != key.images.end(); }, serial);
}

void DescriptorCache::forgetSampler(std::uint32_t id, std::uint64_t serial) {
    forget([id](const ResourceKey& key) { return std::ranges::find(key.samplers, id) != key.samplers.end(); }, serial);
}

void DescriptorCache::trim(std::uint64_t completed) {
    if (resourceSets.size() <= kMaxResourceSets) {
        return;
    }
    for (auto entry = resourceSets.begin(); entry != resourceSets.end();) {
        if (entry->second.used > completed) {
            ++entry;
            continue;
        }
        context.getFunctions().vkFreeDescriptorSets(context.getDevice(), entry->second.pool, 1, &entry->second.set);
        entry = resourceSets.erase(entry);
    }
}

// Freed sets go back to their pools, so every pool is tried before a new one is made.
std::pair<VkDescriptorPool, VkDescriptorSet> DescriptorCache::allocate(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorSetCount = 1, .pSetLayouts = &layout};
    VkDescriptorSet set = VK_NULL_HANDLE;
    for (auto pool = pools.rbegin(); pool != pools.rend(); ++pool) {
        info.descriptorPool = *pool;
        const VkResult result = context.getFunctions().vkAllocateDescriptorSets(context.getDevice(), &info, &set);
        if (result == VK_SUCCESS) {
            return {*pool, set};
        }
        if (result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL) {
            Context::check(result, "allocate a descriptor set");
        }
    }

    pools.push_back(makePool());
    info.descriptorPool = pools.back();
    Context::check(context.getFunctions().vkAllocateDescriptorSets(context.getDevice(), &info, &set), "allocate a descriptor set");
    return {pools.back(), set};
}

VkDescriptorPool DescriptorCache::makePool() const {
    const std::array<VkDescriptorPoolSize, 3> sizes{{
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = kSetsPerPool * 2},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = kSetsPerPool * 4},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = kSetsPerPool * 4},
    }};
    const VkDescriptorPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, .maxSets = kSetsPerPool, .poolSizeCount = static_cast<std::uint32_t>(sizes.size()), .pPoolSizes = sizes.data()};
    VkDescriptorPool pool = VK_NULL_HANDLE;
    Context::check(context.getFunctions().vkCreateDescriptorPool(context.getDevice(), &info, nullptr, &pool), "make a descriptor pool");
    return pool;
}

} // namespace haylen::gpu::vulkan
