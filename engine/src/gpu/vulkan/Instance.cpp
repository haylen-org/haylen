#include "gpu/vulkan/Instance.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <stdexcept>
#include <vector>

#include "gpu/vulkan/Conversions.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::gpu::vulkan {

std::once_flag Instance::loaderOpened;
VkResult Instance::loaderResult = VK_ERROR_INITIALIZATION_FAILED;

Instance::Instance(bool validation) {
    openLoader();
    if (loaderResult != VK_SUCCESS) {
        throw std::runtime_error("Vulkan is not installed on this machine.");
    }

    std::uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion != nullptr) {
        vkEnumerateInstanceVersion(&loaderVersion);
    }
    if (loaderVersion < VK_API_VERSION_1_1) {
        throw std::runtime_error("The Vulkan loader of this machine is older than Vulkan 1.1, which the Vulkan device needs.");
    }
    apiVersion = std::min<std::uint32_t>(loaderVersion, VK_API_VERSION_1_3);

    std::uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());
    // clang-format off
    const auto has = [&](const char* name) {
        return std::ranges::any_of(available, [name](const VkExtensionProperties& extension) { return std::strcmp(extension.extensionName, name) == 0; });
    };
    // clang-format on

    // The extensions of every surface the platform may hand over are on whenever the loader has them, so a device made offscreen takes a window later.
    std::vector<const char*> extensions;
    for (const char* name : {VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
                             VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
                             VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
                             VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
                             VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#endif
                             VK_EXT_DEBUG_UTILS_EXTENSION_NAME}) {
        if (has(name)) {
            extensions.push_back(name);
        }
    }
    debugUtils = has(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> layers;
    if (validation && hasLayer(kValidationLayer)) {
        layers.push_back(kValidationLayer);
    } else if (validation) {
        core::Log::warning("The Vulkan validation layers are not installed, so the Vulkan device runs without them.");
    }

    const VkApplicationInfo application{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "Haylen", .pEngineName = "Haylen", .apiVersion = apiVersion};
    const VkDebugUtilsMessengerCreateInfoEXT messengerInfo = describeMessenger();
    const bool validating = !layers.empty() && debugUtils;
    const VkInstanceCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = validating ? &messengerInfo : nullptr,
        .pApplicationInfo = &application,
        .enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };
    const VkResult result = vkCreateInstance(&info, nullptr, &handle);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("The Vulkan instance could not be created ({}).", Conversions::describe(result)));
    }
    volkLoadInstanceOnly(handle);

    if (validating && vkCreateDebugUtilsMessengerEXT(handle, &messengerInfo, nullptr, &messenger) != VK_SUCCESS) {
        messenger = VK_NULL_HANDLE;
    }
}

Instance::~Instance() {
    if (messenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(handle, messenger, nullptr);
    }
    vkDestroyInstance(handle, nullptr);
}

void Instance::openLoader() {
    std::call_once(loaderOpened, [] { loaderResult = volkInitialize(); });
}

bool Instance::hasLayer(const char* name) {
    std::uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    return std::ranges::any_of(layers, [name](const VkLayerProperties& layer) { return std::strcmp(layer.layerName, name) == 0; });
}

VkDebugUtilsMessengerCreateInfoEXT Instance::describeMessenger() noexcept {
    return {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &Instance::report,
    };
}

// Errors and the warnings of the validation of calls break the rules of Vulkan, so they are errors of the engine, which fail the tests, while the other warnings only advise.
VKAPI_ATTR VkBool32 VKAPI_CALL Instance::report(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*user*/) {
    const bool error = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0 || (types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0;
    if (error) {
        core::Log::error("The Vulkan validation layers report: {}", data->pMessage);
    } else {
        core::Log::warning("The Vulkan validation layers advise: {}", data->pMessage);
    }
    return VK_FALSE;
}

VkSurfaceKHR Instance::createSurface(const Surface& surface) const {
    VkSurfaceKHR made = VK_NULL_HANDLE;
    VkResult result = VK_ERROR_EXTENSION_NOT_PRESENT;
    switch (surface.kind) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    case Surface::Kind::Win32: {
        const VkWin32SurfaceCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, .hinstance = static_cast<HINSTANCE>(surface.display), .hwnd = static_cast<HWND>(surface.window)};
        result = vkCreateWin32SurfaceKHR(handle, &info, nullptr, &made);
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    case Surface::Kind::Xlib: {
        const VkXlibSurfaceCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR, .dpy = static_cast<Display*>(surface.display), .window = static_cast<Window>(reinterpret_cast<std::uintptr_t>(surface.window))};
        result = vkCreateXlibSurfaceKHR(handle, &info, nullptr, &made);
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    case Surface::Kind::Wayland: {
        const VkWaylandSurfaceCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR, .display = static_cast<wl_display*>(surface.display), .surface = static_cast<wl_surface*>(surface.window)};
        result = vkCreateWaylandSurfaceKHR(handle, &info, nullptr, &made);
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    case Surface::Kind::Android: {
        const VkAndroidSurfaceCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .window = static_cast<ANativeWindow*>(surface.window)};
        result = vkCreateAndroidSurfaceKHR(handle, &info, nullptr, &made);
        break;
    }
#endif
    default:
        throw std::runtime_error("The Vulkan device cannot draw into a window of this kind.");
    }
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::format("The Vulkan device could not make the surface of the window ({}).", Conversions::describe(result)));
    }
    return made;
}

} // namespace haylen::gpu::vulkan
