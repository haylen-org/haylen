#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

#include "haylen/platform/Orientation.hpp"

namespace haylen::platform {

// The orientation of the screen of an iPhone or iPad. Until an app locks it, the orientations the Info.plist lists from app.json apply.
class AppleOrientation final {
  public:
    [[nodiscard]] static Orientation get();
    static void lock(Orientation value);

  private:
    static UIInterfaceOrientationMask mask;

    [[nodiscard]] static UIInterfaceOrientationMask toMask(Orientation value);
};

} // namespace haylen::platform
#endif
