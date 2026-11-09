#pragma once

namespace haylen::platform {

// Reports the low power mode of the system to the engine through `NSProcessInfo`, on macOS, iOS, iPadOS, Mac Catalyst, tvOS and visionOS.
class ApplePowerSaving final {
  public:
    // Reports the mode now and after every change that the system announces.
    static void observe();

  private:
    static void report();
};

} // namespace haylen::platform
