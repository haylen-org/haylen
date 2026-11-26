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

    std::vector<const char*> extensions;
    swapchain = has(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (swapchain) {
        extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    // The resolves of depth and the second version of render passes that they need are part of Vulkan 1.2, and extensions before it.
    const bool before12 = apiVersion < VK_API_VERSION_1_2;
    const bool resolveExtensions = before12 && has(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME) && has(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    if (resolveExtensions) {
        extensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
    }

    const bool canResolveDepth = !before12 || resolveExtensions;
    if (canResolveDepth) {
        VkPhysicalDeviceDepthStencilResolveProperties resolveProperties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES, .pNext = nullptr, .supportedDepthResolveModes = 0, .supportedStencilResolveModes = 0, .independentResolveNone = VK_FALSE, .independentResolve = VK_FALSE};
        VkPhysicalDeviceProperties2 described{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &resolveProperties, .properties = {}};
        vkGetPhysicalDeviceProperties2(physicalDevice, &described);
        depthResolveModes = resolveProperties.supportedDepthResolveModes;
    }

    // Dynamic rendering is part of Vulkan 1.3, and before it an extension that needs the resolve of depth.
    const bool renderingExtension = apiVersion < VK_API_VERSION_1_3 && canResolveDepth && has(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    if (renderingExtension) {
        extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    }

    // The display timing of a swapchain tells when the display showed each present on the monotonic clock, which is the steady clock of the engine on Linux and Android alone.
#if defined(__linux__)
    displayTiming = swapchain && has(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME);
    if (displayTiming) {
        extensions.push_back(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME);
    }
#endif

    memoryBudget = has(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    if (memoryBudget) {
        extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }

    // The HDR profile of ASTC is part of Vulkan 1.3, and before it an extension.
    const bool astcHdrExtension = apiVersion < VK_API_VERSION_1_3 && has(VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME);
    const bool canAstcHdr = apiVersion >= VK_API_VERSION_1_3 || astcHdrExtension;

    // A driver that runs Vulkan over another API names the parts of Vulkan it lacks through this extension, which it then requires.
    if (has("VK_KHR_portability_subset")) {
        extensions.push_back("VK_KHR_portability_subset");
    }

    // A present that carries an id lets the device wait until the display shows it, which both extensions bring together.
    const bool presentWaitExtensions = swapchain && has(VK_KHR_PRESENT_ID_EXTENSION_NAME) && has(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);

    VkPhysicalDeviceTextureCompressionASTCHDRFeatures astcHdrFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES, .pNext = nullptr, .textureCompressionASTC_HDR = VK_FALSE};
    VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR, .pNext = nullptr, .presentWait = VK_FALSE};
    VkPhysicalDevicePresentIdFeaturesKHR presentIdFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR, .pNext = &presentWaitFeatures, .presentId = VK_FALSE};
    VkPhysicalDeviceDynamicRenderingFeatures renderingFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES, .pNext = nullptr, .dynamicRendering = VK_FALSE};
    VkPhysicalDeviceFeatures2 supported{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = nullptr, .features = {}};
    const bool canRender = apiVersion >= VK_API_VERSION_1_3 || renderingExtension;
    void** supportedTail = &supported.pNext;
    if (canRender) {
        *supportedTail = &renderingFeatures;
        supportedTail = &renderingFeatures.pNext;
    }

    if (canAstcHdr) {
        *supportedTail = &astcHdrFeatures;
        supportedTail = &astcHdrFeatures.pNext;
    }

    if (presentWaitExtensions) {
        *supportedTail = &presentIdFeatures;
    }

    vkGetPhysicalDeviceFeatures2(physicalDevice, &supported);
    dynamicRendering = canRender && renderingFeatures.dynamicRendering == VK_TRUE;
    if (canRender && !dynamicRendering && renderingExtension) {
        std::erase_if(extensions, [](const char* name) { return std::strcmp(name, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0; });
    }

    astcHdr = canAstcHdr && astcHdrFeatures.textureCompressionASTC_HDR == VK_TRUE;
    if (astcHdr && astcHdrExtension) {
        extensions.push_back(VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME);
    }

    presentWait = presentWaitExtensions && presentIdFeatures.presentId == VK_TRUE && presentWaitFeatures.presentWait == VK_TRUE;
    if (presentWait) {
        extensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
        extensions.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
    }

    // The block compressed formats of each family sample only on a device made with its feature, and so do cube arrays.
    enabledFeatures.samplerAnisotropy = supported.features.samplerAnisotropy;
    enabledFeatures.imageCubeArray = supported.features.imageCubeArray;
    enabledFeatures.independentBlend = supported.features.independentBlend;
    enabledFeatures.textureCompressionBC = supported.features.textureCompressionBC;
    enabledFeatures.textureCompressionETC2 = supported.features.textureCompressionETC2;
    enabledFeatures.textureCompressionASTC_LDR = supported.features.textureCompressionASTC_LDR;
    VkPhysicalDeviceTextureCompressionASTCHDRFeatures enabledAstcHdr{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES, .pNext = nullptr, .textureCompressionASTC_HDR = VK_TRUE};
    VkPhysicalDevicePresentWaitFeaturesKHR enabledPresentWait{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR, .pNext = nullptr, .presentWait = VK_TRUE};
    VkPhysicalDevicePresentIdFeaturesKHR enabledPresentId{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR, .pNext = &enabledPresentWait, .presentId = VK_TRUE};
    VkPhysicalDeviceDynamicRenderingFeatures enabledRendering{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES, .pNext = nullptr, .dynamicRendering = VK_TRUE};
    VkPhysicalDeviceFeatures2 enabled{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = nullptr, .features = enabledFeatures};
    void** enabledTail = &enabled.pNext;
    if (dynamicRendering) {
        *enabledTail = &enabledRendering;
        enabledTail = &enabledRendering.pNext;
    }

    if (astcHdr) {
        *enabledTail = &enabledAstcHdr;
        enabledTail = &enabledAstcHdr.pNext;
    }

    if (presentWait) {
        *enabledTail = &enabledPresentId;
    }

    const float priority = 1.0F;
    const VkDeviceQueueCreateInfo queueInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .pNext = nullptr, .flags = 0, .queueFamilyIndex = queueFamily, .queueCount = 1, .pQueuePriorities = &priority};
    const VkDeviceCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabled,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
        .pEnabledFeatures = nullptr,
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
        .preferredLargeHeapBlockSize = 0,
        .pAllocationCallbacks = nullptr,
        .pDeviceMemoryCallbacks = nullptr,
        .pHeapSizeLimit = nullptr,
        .pVulkanFunctions = &entryPoints,
        .instance = instance,
        .vulkanApiVersion = apiVersion,
        .pTypeExternalMemoryHandleTypes = nullptr,
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
