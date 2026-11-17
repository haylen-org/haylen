#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/DeviceOptions.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::port {

// Makes the GPU devices of the graphics APIs that the build compiled, which the runtime of a port opens its window for in the order of `Player::getGraphicsApis`.
class GpuDevices final {
  public:
    // Starts what the API needs before `create` that arrives later, and returns whether `create` can run now.
    [[nodiscard]] static bool prepare(gpu::Device::Api api, const gpu::DeviceOptions& options);

    // Creates the device of the API, drawing into the surface or offscreen when there is none. Throws `std::runtime_error` with a clear sentence when the API cannot start, so the runtime tries the next one.
    [[nodiscard]] static std::unique_ptr<gpu::Device> create(gpu::Device::Api api, const std::optional<gpu::Surface>& surface, const gpu::DeviceOptions& options);

    // A device that draws nothing and checks every call of the engine against the contract of `gpu::Device`, reporting the API and running the shader language it is given, on which the runtime of a port plays apps before its own device draws.
    [[nodiscard]] static std::unique_ptr<gpu::Device> createChecking(gpu::Device::Api api, gpu::Device::Language language);

    // The name of an API in `gpu::Device::kApiNames`, such as `port`.
    [[nodiscard]] static std::string_view getApiName(gpu::Device::Api api) noexcept;
};

} // namespace haylen::port
