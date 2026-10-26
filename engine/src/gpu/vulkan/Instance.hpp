#pragma once

#include <cstdint>
#include <mutex>

#include <volk.h>

#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::vulkan {

// The Vulkan instance of a device. It opens the Vulkan loader of the system at run time, so nothing of Vulkan is needed to build or start the engine, turns on the extensions that make surfaces of the windows of the platform and the labels of GPU debuggers, and, when the device validates, the validation layers, whose messages become errors of the engine log.
class Instance final {
  public:
    explicit Instance(bool validation);
    ~Instance();
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    [[nodiscard]] VkInstance getHandle() const noexcept {
        return handle;
    }

    [[nodiscard]] std::uint32_t getApiVersion() const noexcept {
        return apiVersion;
    }

    [[nodiscard]] bool hasDebugUtils() const noexcept {
        return debugUtils;
    }

    // Makes the Vulkan surface of the window, which the caller destroys with `vkDestroySurfaceKHR`.
    [[nodiscard]] VkSurfaceKHR createSurface(const Surface& surface) const;

  private:
    static constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

    static void openLoader();
    [[nodiscard]] static bool hasLayer(const char* name);
    static VKAPI_ATTR VkBool32 VKAPI_CALL report(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user);
    [[nodiscard]] static VkDebugUtilsMessengerCreateInfoEXT describeMessenger() noexcept;

    static std::once_flag loaderOpened;
    static VkResult loaderResult;

    VkInstance handle = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    std::uint32_t apiVersion = VK_API_VERSION_1_1;
    bool debugUtils = false;
};

} // namespace haylen::gpu::vulkan
