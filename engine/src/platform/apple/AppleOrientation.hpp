#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

#include "haylen/platform/Orientation.hpp"
#include "platform/SupportedOrientations.hpp"

namespace haylen::platform {

// The orientation of the screen of an iPhone or iPad. Until an app locks it, the orientations of its `app.json` apply.
class AppleOrientation final {
  public:
    [[nodiscard]] static Orientation get();
    static void lock(Orientation value);

    // Takes the orientation of `app.json` before the window of an app opens.
    static void prepare(Orientation value);

    // The orientations the app allows now, which the application delegate and the view controller of the window answer UIKit with.
    [[nodiscard]] static UIInterfaceOrientationMask getSupported();

  private:
    [[nodiscard]] static SupportedOrientations& getOrientations();
};

} // namespace haylen::platform
#endif
