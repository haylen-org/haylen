#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

#include <volk.h>

namespace haylen::gpu::vulkan {

class Context;

// The descriptor set layouts and pipeline layouts of the programs of a device, made once for each distinct description, so programs that bind the same things share them and their descriptor sets. They live as long as the device.
class LayoutCache final {
  public:
    struct Binding {
        std::uint32_t binding = 0;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_SAMPLER;
        VkShaderStageFlags stages = 0;

        friend auto operator<=>(const Binding&, const Binding&) = default;
    };

    explicit LayoutCache(const Context& owner) noexcept : context(owner) {}
    ~LayoutCache();
    LayoutCache(const LayoutCache&) = delete;
    LayoutCache& operator=(const LayoutCache&) = delete;

    // The layout of a set with the bindings, in the order of their binding numbers.
    [[nodiscard]] VkDescriptorSetLayout getSetLayout(std::span<const Binding> bindings);
    [[nodiscard]] VkPipelineLayout getPipelineLayout(std::span<const VkDescriptorSetLayout> sets);

  private:
    const Context& context;
    std::map<std::vector<Binding>, VkDescriptorSetLayout> setLayouts;
    std::map<std::vector<std::uint64_t>, VkPipelineLayout> pipelineLayouts;
};

} // namespace haylen::gpu::vulkan
