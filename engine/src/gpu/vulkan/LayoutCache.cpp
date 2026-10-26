#include "gpu/vulkan/LayoutCache.hpp"

#include "gpu/vulkan/Context.hpp"

namespace haylen::gpu::vulkan {

LayoutCache::~LayoutCache() {
    const VolkDeviceTable& vk = context.getFunctions();
    for (const auto& [key, layout] : pipelineLayouts) {
        vk.vkDestroyPipelineLayout(context.getDevice(), layout, nullptr);
    }
    for (const auto& [key, layout] : setLayouts) {
        vk.vkDestroyDescriptorSetLayout(context.getDevice(), layout, nullptr);
    }
}

VkDescriptorSetLayout LayoutCache::getSetLayout(std::span<const Binding> bindings) {
    std::vector<Binding> key(bindings.begin(), bindings.end());
    if (const auto found = setLayouts.find(key); found != setLayouts.end()) {
        return found->second;
    }

    std::vector<VkDescriptorSetLayoutBinding> described;
    described.reserve(bindings.size());
    for (const Binding& binding : bindings) {
        described.push_back({.binding = binding.binding, .descriptorType = binding.type, .descriptorCount = 1, .stageFlags = binding.stages});
    }
    const VkDescriptorSetLayoutCreateInfo info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = static_cast<std::uint32_t>(described.size()), .pBindings = described.data()};
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    Context::check(context.getFunctions().vkCreateDescriptorSetLayout(context.getDevice(), &info, nullptr, &layout), "make a descriptor set layout");
    setLayouts.emplace(std::move(key), layout);
    return layout;
}

VkPipelineLayout LayoutCache::getPipelineLayout(std::span<const VkDescriptorSetLayout> sets) {
    std::vector<std::uint64_t> key;
    key.reserve(sets.size());
    for (VkDescriptorSetLayout set : sets) {
        key.push_back(Context::toHandle(set));
    }
    if (const auto found = pipelineLayouts.find(key); found != pipelineLayouts.end()) {
        return found->second;
    }

    const VkPipelineLayoutCreateInfo info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = static_cast<std::uint32_t>(sets.size()), .pSetLayouts = sets.data()};
    VkPipelineLayout layout = VK_NULL_HANDLE;
    Context::check(context.getFunctions().vkCreatePipelineLayout(context.getDevice(), &info, nullptr, &layout), "make a pipeline layout");
    pipelineLayouts.emplace(std::move(key), layout);
    return layout;
}

} // namespace haylen::gpu::vulkan
