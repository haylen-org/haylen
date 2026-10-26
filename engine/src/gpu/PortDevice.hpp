#pragma once

#include <memory>
#include <optional>

#include "gpu/DeviceFactory.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu {

// The device of the graphics API `Api::Port`, which a port of the engine to another platform defines in the folder that `HAYLEN_PORT_DIR` names when the engine has no device of the API of that platform.
class PortDevice final {
  public:
    // Creates the device of the port, drawing into the surface or offscreen when there is none. Throws `std::runtime_error` with a clear sentence when the API cannot start, so the platform tries the next one.
    [[nodiscard]] static std::unique_ptr<Device> create(const std::optional<Surface>& surface, const DeviceFactory::Options& options);
};

} // namespace haylen::gpu
