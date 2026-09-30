#pragma once

#include <TargetConditionals.h>

#include "haylen/platform/Battery.hpp"

namespace haylen::platform {

// Reports the battery to the engine: the power sources of IOKit on macOS and Mac Catalyst, the battery monitoring of `UIDevice` on iOS and iPadOS, and no battery on tvOS, since TVs run on mains power.
class AppleBattery final {
  public:
    // Reports the battery now and after every change that the system announces.
    static void observe();

  private:
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    // The internal battery among the power sources, or none on Macs that run on mains power alone.
    [[nodiscard]] static Battery readPowerSources();

    // The callback of the run loop source that IOKit signals whenever a power source changes.
    static void powerSourcesChanged(void* context);
#elif TARGET_OS_IOS
    [[nodiscard]] static Battery readDevice();
#endif
};

} // namespace haylen::platform
