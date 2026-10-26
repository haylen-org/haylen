#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include <volk.h>

#include <vk_mem_alloc.h>

namespace haylen::gpu::vulkan {

class Instance;

// The GPU that a Vulkan device draws with: the physical device it picked, the logical device with its one queue for graphics, transfers and presentation, the table of its functions, the memory allocator, and what the GPU and its driver offer.
class Context final {
  public:
    // Picks the fastest GPU that runs Vulkan 1.1 and, when a surface is given, presents to it.
    Context(const Instance& instance, VkSurfaceKHR surface);
    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    [[nodiscard]] VkInstance getInstance() const noexcept {
        return instance;
    }

    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const noexcept {
        return physicalDevice;
    }

    [[nodiscard]] VkDevice getDevice() const noexcept {
        return device;
    }

    [[nodiscard]] VkQueue getQueue() const noexcept {
        return queue;
    }

    [[nodiscard]] std::uint32_t getQueueFamily() const noexcept {
        return queueFamily;
    }

    [[nodiscard]] VmaAllocator getAllocator() const noexcept {
        return allocator;
    }

    [[nodiscard]] const VolkDeviceTable& getFunctions() const noexcept {
        return functions;
    }

    [[nodiscard]] const VkPhysicalDeviceProperties& getProperties() const noexcept {
        return properties;
    }

    [[nodiscard]] const VkPhysicalDeviceFeatures& getEnabledFeatures() const noexcept {
        return enabledFeatures;
    }

    [[nodiscard]] std::uint32_t getApiVersion() const noexcept {
        return apiVersion;
    }

    [[nodiscard]] bool hasDynamicRendering() const noexcept {
        return dynamicRendering;
    }

    [[nodiscard]] bool hasSwapchain() const noexcept {
        return swapchain;
    }

    // How many bits of a timestamp of the queue count, or none where the queue measures no time.
    [[nodiscard]] std::uint32_t getTimestampBits() const noexcept {
        return timestampBits;
    }

    // The depth and stencil format of Vulkan that takes the place of `Depth24Stencil8`, which some GPUs lack.
    [[nodiscard]] VkFormat getDepthStencilFormat() const noexcept {
        return depthStencilFormat;
    }

    [[nodiscard]] bool canPresent(VkSurfaceKHR surface) const;
    [[nodiscard]] bool hasFormatFeatures(VkFormat format, VkFormatFeatureFlags features) const;

    // Names an object in the captures of GPU debuggers and in the messages of the validation layers.
    template <typename Handle> void setName(VkObjectType type, Handle handle, std::string_view name) const {
        if (!debugUtils || name.empty()) {
            return;
        }
        nameText.assign(name);
        const VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, .objectType = type, .objectHandle = toHandle(handle), .pObjectName = nameText.c_str()};
        vkSetDebugUtilsObjectNameEXT(device, &info);
    }

    // The value of a handle of Vulkan as the 64 bits that debuggers and caches read, since the non-dispatchable handles are pointers on 64-bit systems and integers on 32-bit ones.
    template <typename Handle> [[nodiscard]] static std::uint64_t toHandle(Handle handle) noexcept {
        if constexpr (std::is_pointer_v<Handle>) {
            return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
        } else {
            return static_cast<std::uint64_t>(handle);
        }
    }

    // Throws the failure of a call of Vulkan as a sentence that names what failed.
    static void check(VkResult result, std::string_view action);

  private:
    struct Candidate {
        VkPhysicalDevice device = VK_NULL_HANDLE;
        std::uint32_t queueFamily = 0;
        int score = -1;
    };

    [[nodiscard]] static Candidate rate(VkPhysicalDevice candidate, VkSurfaceKHR surface);
    void createDevice();
    void createAllocator();

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VolkDeviceTable functions{};
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceFeatures enabledFeatures{};
    std::uint32_t apiVersion = VK_API_VERSION_1_1;
    std::uint32_t timestampBits = 0;
    VkFormat depthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    bool dynamicRendering = false;
    bool swapchain = false;
    bool memoryBudget = false;
    bool debugUtils = false;
    // The text of the last name given to an object, kept so naming never allocates once it holds the longest name.
    mutable std::string nameText;
};

} // namespace haylen::gpu::vulkan
