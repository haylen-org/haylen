#pragma once

#include <TargetConditionals.h>

#include "haylen/platform/AccessibilitySettings.hpp"

namespace haylen::platform {

// Reports the accessibility settings of the system to the engine: the display options of `NSWorkspace` on macOS, whose VoiceOver state the application delegate follows with key-value observing, and the settings of `UIAccessibility` with the preferred content size of the app on iOS, iPadOS, tvOS and Mac Catalyst.
class AppleAccessibility final {
  public:
    // Reports the settings now and after every change that the system announces.
    static void observe();
    static void report();

  private:
    [[nodiscard]] static AccessibilitySettings read();
};

} // namespace haylen::platform
