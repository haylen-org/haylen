#pragma once

namespace haylen::platform {

// Follows the network path of the device with NWPathMonitor and reports whether it reaches the network, first when the monitor starts and then on every change.
class AppleNetwork final {
  public:
    static void observe();
};

} // namespace haylen::platform
