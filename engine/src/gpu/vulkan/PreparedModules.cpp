#include "gpu/vulkan/PreparedModules.hpp"

#include <algorithm>

#include "gpu/vulkan/Context.hpp"

namespace haylen::gpu::vulkan {

PreparedModules::~PreparedModules() {
    close();
}

// A module of the same code may already wait, and the device may close while the module is made, which the lock keeps apart.
void PreparedModules::prepare(std::span<const std::uint32_t> code) {
    const std::scoped_lock lock(mutex);
    if (context == nullptr || std::ranges::any_of(modules, [code](const auto& entry) { return std::ranges::equal(entry.first, code); })) {
        return;
    }
    const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = code.size_bytes(), .pCode = code.data()};
    VkShaderModule module = VK_NULL_HANDLE;
    if (context->getFunctions().vkCreateShaderModule(context->getDevice(), &info, nullptr, &module) == VK_SUCCESS) {
        modules.emplace_back(std::vector<std::uint32_t>(code.begin(), code.end()), module);
    }
}

VkShaderModule PreparedModules::take(std::span<const std::uint32_t> code) {
    const std::scoped_lock lock(mutex);
    const auto found = std::ranges::find_if(modules, [code](const auto& entry) { return std::ranges::equal(entry.first, code); });
    if (found == modules.end()) {
        return VK_NULL_HANDLE;
    }
    const VkShaderModule module = found->second;
    modules.erase(found);
    return module;
}

void PreparedModules::close() {
    const std::scoped_lock lock(mutex);
    destroyAll();
    context = nullptr;
}

void PreparedModules::destroyAll() {
    if (context == nullptr) {
        return;
    }
    for (const auto& [code, module] : modules) {
        context->getFunctions().vkDestroyShaderModule(context->getDevice(), module, nullptr);
    }
    modules.clear();
}

} // namespace haylen::gpu::vulkan
