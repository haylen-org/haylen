#pragma once

#include <memory>
#include <optional>

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/DeviceOptions.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::port {

// The device of the graphics API `Api::Port`, which a port defines in its `DEVICE_SOURCES` when the engine has no device of the graphics API of its platform. It reports `Api::Port` and `Language::Port` and reads the code that the `SHADER_COMMAND` of the port wrote into the shader packages.
class PortDevice final {
  public:
    // Creates the device of the port, drawing into the surface, whose kind is `Surface::Kind::Port`, or offscreen when there is none. Throws `std::runtime_error` with a clear sentence when the API cannot start, so the platform tries the next one.
    [[nodiscard]] static std::unique_ptr<gpu::Device> create(const std::optional<gpu::Surface>& surface, const gpu::DeviceOptions& options);
};

} // namespace haylen::port
