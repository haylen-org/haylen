#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The drivers whose bugs the devices work around, each with the reason that the tests on its GPU showed. An entry names the graphics API, a piece of the name the driver gives its GPU, and the versions that have the bug where they are known, as a piece of the version text of the driver or as the last version number where the API numbers its drivers, so a newer driver that fixed the bug leaves the entry.
class DriverIssues final {
  public:
    enum class Issue : std::uint8_t {
        // The device of the API refuses to start, so the platform draws through the next API of its order.
        AvoidApi,
        // The 32-bit float formats are not marked filterable, although the driver names an extension that says they are.
        NoFloatFiltering,
        // Every pass ends with a flush, which hands its commands to the GPU before the next pass records.
        FlushEachPass,
        // A pass that keeps the depth of a format without stencil also stores the depth and keeps the stencil it has no plane for.
        StoreKeptDepth,
        // The GPU timer reads no disjoint state, since the driver names the extension of disjoint timer queries but refuses its `GL_GPU_DISJOINT_EXT`.
        NoGpuDisjoint,
        // The context compiles shaders and links programs on the thread that asks for them, without the compiler threads of the driver.
        CompileOnCallingThread,
        // The clears of a pass reach only the first layer of an array or a volume texture, so a pass clears an attachment at a later layer by drawing.
        ClearLayersByDrawing,
        // Textures filter without anisotropy, since the anisotropic filtering of the driver ignores the slopes that a shader gives a lookup.
        NoAnisotropy,
        // The swapchain waits for its next image without a timeout, since its platform takes no other wait and warns at every bounded one.
        UnboundedAcquire,
        // Textures of 32-bit float depth are made with a stencil plane, since the lookups of the driver read the depth that its draws write into a texture of 32-bit float depth alone in other places.
        MakeFloatDepthWithStencil,
    };

    // A driver as its graphics API describes it: the name of its GPU, its version text and its version number, which is 0 where the API gives none.
    struct Driver {
        Device::Api api = Device::Api::None;
        std::string_view renderer;
        std::string_view version{};
        std::uint64_t versionNumber = 0;
    };

    // The reason of the entry of the issue that matches the driver, or nothing when its driver has no such issue.
    [[nodiscard]] static std::optional<std::string_view> find(const Driver& driver, Issue issue) noexcept;

    // The version number of a driver of Mesa from the version text of its OpenGL or OpenGL ES, such as `4.5 (Core Profile) Mesa 26.0.8`, made as Mesa numbers its Vulkan drivers, or 0 for the text of another driver.
    [[nodiscard]] static std::uint64_t readMesaVersion(std::string_view version) noexcept;

  private:
    struct Entry {
        Device::Api api = Device::Api::None;
        Issue issue = Issue::AvoidApi;
        std::string_view renderer;
        std::string_view version{};
        std::uint64_t lastVersionNumber = 0;
        std::string_view reason;
    };

    static const std::array<Entry, 10> kEntries;

    // Mesa numbers its Vulkan drivers with 10 bits for the minor version and 12 bits for the patch, so larger parts saturate there.
    [[nodiscard]] static constexpr std::uint64_t makeMesaVersion(std::uint64_t major, std::uint64_t minor, std::uint64_t patch) noexcept {
        return (major << 22U) | (std::min<std::uint64_t>(minor, 1023) << 12U) | std::min<std::uint64_t>(patch, 4095);
    }

    [[nodiscard]] static bool matches(const Entry& entry, const Driver& driver, Issue issue) noexcept;
};

} // namespace haylen::gpu
