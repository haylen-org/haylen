#include "gpu/vulkan/PreparedModules.hpp"

#include <algorithm>

#include "gpu/vulkan/Context.hpp"

namespace haylen::gpu::vulkan {

PreparedModules::~PreparedModules() {
    close();
}

// A module of the same code may already wait or be in the making, and the device closes only once the modules in the making are made, so the context stays while one is made.
void PreparedModules::prepare(std::span<const std::uint32_t> code) {
    {
        const std::scoped_lock lock(mutex);
        if (context == nullptr || std::ranges::any_of(modules, [code](const auto& entry) { return std::ranges::equal(entry.first, code); })) {
            return;
        }

        modules.emplace_back(std::vector<std::uint32_t>(code.begin(), code.end()), VK_NULL_HANDLE);
        ++making;
    }

    const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .pNext = nullptr, .flags = 0, .codeSize = code.size_bytes(), .pCode = code.data()};
    VkShaderModule module = VK_NULL_HANDLE;
    const VkResult result = context->getFunctions().vkCreateShaderModule(context->getDevice(), &info, nullptr, &module);

    // A program that took the code while the module was in the making made its own, and a module that failed leaves the program to make it and report why.
    const std::scoped_lock lock(mutex);
    const auto found = std::ranges::find_if(modules, [code](const auto& entry) { return std::ranges::equal(entry.first, code) && entry.second == VK_NULL_HANDLE; });
    if (result == VK_SUCCESS && found != modules.end()) {
        found->second = module;
    } else {
        if (result == VK_SUCCESS) {
            context->getFunctions().vkDestroyShaderModule(context->getDevice(), module, nullptr);
        }

        if (found != modules.end()) {
            modules.erase(found);
        }
    }

    --making;
    made.notify_all();
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
    std::unique_lock lock(mutex);
    made.wait(lock, [this] { return making == 0; });
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
