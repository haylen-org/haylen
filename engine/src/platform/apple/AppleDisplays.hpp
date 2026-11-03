#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

#include <string>
#include <vector>

#include "haylen/platform/Display.hpp"

namespace haylen::platform {

// The screens of iPhones, iPads and TVs: the screen of the device and every other screen that a scene of the app is on, such as an external display that an iPad extends its windows to or that shows the scene of an external display. UIKit gives screens no lasting identity, so the screen of the device is `main` and every other screen gets `external-<n>` the first time the app sees it, which it keeps while it stays connected. visionOS has no screens and shows the app in a window of the space around the person, so its one display is that window, as `main`. Everything runs on the main thread.
class AppleDisplays final {
  public:
    [[nodiscard]] static std::vector<Display> getDisplays();
    [[nodiscard]] static std::string getWindowDisplayId();

    // Tells the running app that a screen connected, disconnected or changed, once a scene connected to it or left it.
    static void changed();

#if !TARGET_OS_VISION
    [[nodiscard]] static NSString* getId(UIScreen* screen);

  private:
    [[nodiscard]] static Display describe(UIScreen* screen);

    static NSMapTable<UIScreen*, NSString*>* ids;
    static NSUInteger counted;
#endif
};

} // namespace haylen::platform
#endif
