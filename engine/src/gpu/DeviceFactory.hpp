#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu {

// Makes the device of each graphics API that the build compiled, which `HAYLEN_GRAPHICS_APIS` lists, and orders the APIs a platform tries when it opens its window.
class DeviceFactory final {
  public:
    struct Options {
        // Whether the device turns on the validation of its graphics API and the labels of its objects, which Debug builds and tests do.
        bool validation = false;

        // Whether the device prefers the software rasterizer of its graphics API, such as WARP on Direct3D, which draws where no GPU does and the same pixels on every machine. A device whose API has no software rasterizer ignores it.
        bool software = false;

        // Whether the device prefers the GPU that uses the least power, such as the integrated GPU of a laptop that has two, to the most powerful one. A device whose API does not tell its GPUs apart ignores it.
        bool lowPower = false;
    };

    // The APIs this build compiled, in the default order of preference of the platforms: Metal, Direct3D 12, Direct3D 11, Vulkan, WebGPU, OpenGL, OpenGL ES and WebGL2.
    [[nodiscard]] static std::span<const Device::Api> getCompiledApis() noexcept;

    // The APIs to try in order: the preferred ones that the build compiled, then the other compiled ones in the default order.
    [[nodiscard]] static std::vector<Device::Api> order(std::span<const Device::Api> preferred);

    // Whether the API draws through a `GlContext` that the platform creates with the window.
    [[nodiscard]] static bool usesGlContext(Device::Api api) noexcept;

    // Creates the device of the API, drawing into the surface or offscreen when there is none. Throws `std::runtime_error` with a clear sentence when the API cannot start on this machine, so the platform tries the next one.
    [[nodiscard]] static std::unique_ptr<Device> create(Device::Api api, const std::optional<Surface>& surface, const Options& options);

    // The name of an API in `Device::kApiNames`, such as `vulkan`.
    [[nodiscard]] static std::string_view getApiName(Device::Api api) noexcept;

    // The API of a name in `Device::kApiNames`, or nothing for a name that is not one or names none.
    [[nodiscard]] static std::optional<Device::Api> findApi(std::string_view name) noexcept;

  private:
    [[nodiscard]] static std::vector<Device::Api> listCompiledApis();

    static const std::vector<Device::Api> kCompiledApis;
};

} // namespace haylen::gpu
