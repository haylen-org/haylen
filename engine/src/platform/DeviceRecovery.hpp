#pragma once

#include <optional>
#include <string>

#include "haylen/gpu/Device.hpp"

namespace haylen::platform {

// What a platform does with a GPU device that stopped working. The first loss gives the app a new device, on which it starts again, and a loss of that replacement shows a GPU that keeps no device for the app, which then ends with the reason.
class DeviceRecovery final {
  public:
    // Logs that the device of the graphics API stopped working, and returns the reason the app ends with when the device replaced a lost one, or nothing when the app starts again on a new device.
    [[nodiscard]] std::optional<std::string> recordLoss(gpu::Device::Api api);

  private:
    bool replaced = false;
};

} // namespace haylen::platform
