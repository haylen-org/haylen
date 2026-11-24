#pragma once

namespace haylen::platform {

// Reports how hot the device runs to the engine through the thermal state of `NSProcessInfo`, on macOS, iOS, iPadOS, Mac Catalyst, tvOS and visionOS.
class AppleThermalState final {
  public:
    // Reports the state now and after every change that the system announces.
    static void observe();

  private:
    static void report();
};

} // namespace haylen::platform
