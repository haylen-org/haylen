#include "gpu/vulkan/Context.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/DriverIssues.hpp"
#include "gpu/vulkan/Conversions.hpp"
#include "gpu/vulkan/Instance.hpp"

namespace haylen::gpu::vulkan {

Context::Context(const Instance& owner, VkSurfaceKHR surface, Preference preference) : instance(owner.getHandle()), debugUtils(owner.hasDebugUtils()) {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    Candidate best;
    std::string_view avoided;
    for (VkPhysicalDevice candidate : devices) {
        const Candidate rated = rate(candidate, surface, preference);
        avoided = rated.avoided.empty() ? avoided : rated.avoided;
        if (rated.score > best.score) {
            best = rated;
        }
    }
    if (best.score < 0 && !avoided.empty()) {
        throw std::runtime_error(std::string(avoided));
    }
    if (best.score < 0) {
        throw std::runtime_error(surface == VK_NULL_HANDLE ? "No GPU of this machine runs Vulkan 1.1." : "No GPU of this machine runs Vulkan 1.1 and presents to the window.");
    }
    physicalDevice = best.device;
    queueFamily = best.queueFamily;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    apiVersion = std::min(properties.apiVersion, owner.getApiVersion());

    createDevice();
    createAllocator();
}

Context::~Context() {
    vmaDestroyAllocator(allocator);
    functions.vkDestroyDevice(device, nullptr);
}

// A GPU needs Vulkan 1.1, a driver without a known issue that rules Vulkan out and a queue that draws, and presents to the surface when there is one.
Context::Candidate Context::rate(VkPhysicalDevice candidate, VkSurfaceKHR surface, Preference preference) {
    VkPhysicalDeviceProperties described{};
    vkGetPhysicalDeviceProperties(candidate, &described);
    if (described.apiVersion < VK_API_VERSION_1_1) {
        return {};
    }
    const DriverIssues::Driver driver{.api = gpu::Device::Api::Vulkan, .renderer = described.deviceName, .versionNumber = described.driverVersion};
    if (const std::optional<std::string_view> reason = DriverIssues::find(driver, DriverIssues::Issue::AvoidApi)) {
        return {.avoided = *reason};
    }

    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());
    for (std::uint32_t family = 0; family < count; ++family) {
        if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
            continue;
        }
        VkBool32 presents = VK_TRUE;
        if (surface != VK_NULL_HANDLE && vkGetPhysicalDeviceSurfaceSupportKHR(candidate, family, surface, &presents) != VK_SUCCESS) {
            presents = VK_FALSE;
        }
        if (presents == VK_FALSE) {
            continue;
        }

        return {.device = candidate, .queueFamily = family, .score = score(described.deviceType, preference)};
    }
    return {};
}

// Discrete GPUs come first, then integrated, virtual and software ones, while a preference for low power puts integrated GPUs first and one for software puts the rasterizers that run on the CPU first.
int Context::score(VkPhysicalDeviceType type, Preference preference) noexcept {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return preference.lowPower ? 3 : 4;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return preference.lowPower ? 4 : 3;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return preference.software ? 5 : 1;
    default:
        return 0;
    }
}

void Context::createDevice() {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, available.data());
    // clang-format off
    const auto has = [&](const char* name) {
        return std::ranges::any_of(available, [name](const VkExtensionProperties& extension) { return std::strcmp(extension.extensionName, name) == 0; });
    };
    // clang-format on

    // Dynamic rendering is part of Vulkan 1.3, and before it an extension that needs the resolve of depth and the second version of render passes, which Vulkan 1.2 holds.
    std::vector<const char*> extensions;
    swapchain = has(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (swapchain) {
        extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    bool renderingExtension = false;
    if (apiVersion < VK_API_VERSION_1_3 && has(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) {
        const bool before12 = apiVersion < VK_API_VERSION_1_2;
        renderingExtension = !before12 || (has(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME) && has(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME));
        if (renderingExtension && before12) {
            extensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
            extensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
        }
        if (renderingExtension) {
            extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }
    }
    memoryBudget = has(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    if (memoryBudget) {
        extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }
    // A driver that runs Vulkan over another API names the parts of Vulkan it lacks through this extension, which it then requires.
    if (has("VK_KHR_portability_subset")) {
        extensions.push_back("VK_KHR_portability_subset");
    }

    VkPhysicalDeviceDynamicRenderingFeatures renderingFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
    VkPhysicalDeviceFeatures2 supported{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    const bool canRender = apiVersion >= VK_API_VERSION_1_3 || renderingExtension;
    if (canRender) {
        supported.pNext = &renderingFeatures;
    }
    vkGetPhysicalDeviceFeatures2(physicalDevice, &supported);
    dynamicRendering = canRender && renderingFeatures.dynamicRendering == VK_TRUE;
    if (canRender && !dynamicRendering && renderingExtension) {
        std::erase_if(extensions, [](const char* name) { return std::strcmp(name, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0; });
    }

    enabledFeatures.samplerAnisotropy = supported.features.samplerAnisotropy;
    enabledFeatures.independentBlend = supported.features.independentBlend;
    VkPhysicalDeviceDynamicRenderingFeatures enabledRendering{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES, .dynamicRendering = VK_TRUE};
    const VkPhysicalDeviceFeatures2 enabled{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = dynamicRendering ? &enabledRendering : nullptr, .features = enabledFeatures};

    const float priority = 1.0F;
    const VkDeviceQueueCreateInfo queueInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = queueFamily, .queueCount = 1, .pQueuePriorities = &priority};
    const VkDeviceCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabled,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };
    check(vkCreateDevice(physicalDevice, &info, nullptr, &device), "create its logical device");
    volkLoadDeviceTable(&functions, device);
    functions.vkGetDeviceQueue(device, queueFamily, 0, &queue);

    std::vector<VkQueueFamilyProperties> families(queueFamily + 1);
    std::uint32_t familyCount = queueFamily + 1;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());
    timestampBits = families[queueFamily].timestampValidBits;

    depthStencilFormat = hasFormatFeatures(VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;
}

// The allocator finds the functions of Vulkan itself through the two entry points of the loader.
void Context::createAllocator() {
    VmaVulkanFunctions entryPoints{};
    entryPoints.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    entryPoints.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    const VmaAllocatorCreateInfo info{
        .flags = memoryBudget ? static_cast<VmaAllocatorCreateFlags>(VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT) : 0U,
        .physicalDevice = physicalDevice,
        .device = device,
        .pVulkanFunctions = &entryPoints,
        .instance = instance,
        .vulkanApiVersion = apiVersion,
    };
    check(vmaCreateAllocator(&info, &allocator), "create its memory allocator");
}

bool Context::canPresent(VkSurfaceKHR surface) const {
    VkBool32 presents = VK_FALSE;
    return vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamily, surface, &presents) == VK_SUCCESS && presents == VK_TRUE;
}

bool Context::hasFormatFeatures(VkFormat format, VkFormatFeatureFlags features) const {
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
    return (formatProperties.optimalTilingFeatures & features) == features;
}

void Context::check(VkResult result, std::string_view action) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("The Vulkan device could not {} ({}).", action, Conversions::describe(result)));
    }
}

} // namespace haylen::gpu::vulkan
