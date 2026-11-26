#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

#include <optional>
#include <string>
#include <vector>

#include "haylen/platform/Display.hpp"

namespace haylen::platform {

// The screens of iPhones, iPads and TVs: the main screen of the device and every other screen that a scene of the app is on, such as the inner screen of a foldable iPhone, an external display that an iPad extends its windows to or one that shows the scene of an external display. UIKit gives screens no lasting identity, so the main screen is `main` and every other screen gets `screen-<n>` the first time the app sees it, which it keeps while it stays connected. visionOS has no screens and shows the app in a window of the space around the person, so its one display is that window, as `main`. Everything runs on the main thread.
class AppleDisplays final {
  public:
    [[nodiscard]] static std::vector<Display> getDisplays();
    [[nodiscard]] static std::string getWindowDisplayId();

    // Tells the running app that a screen connected, disconnected or changed, once a scene connected to it or left it.
    static void changed();

    // Tells the running app that the display of the window reads differently after a layout of the window: the screen of the window on an iPhone or an iPad, whose bounds and work area turn with the interface, and on visionOS the window itself, whose size and safe area follow the person who resizes it. The view controller of the window calls it with every layout, on the main thread.
    static void followWindow();

#if !TARGET_OS_VISION
    [[nodiscard]] static NSString* getId(UIScreen* screen);
#endif

  private:
    // The display that shows the window, or nothing before the window is in a scene.
    [[nodiscard]] static std::optional<Display> describeWindowDisplay();

#if !TARGET_OS_VISION
    [[nodiscard]] static Display describe(UIScreen* screen);

    static NSMapTable<UIScreen*, NSString*>* ids;
    static NSUInteger counted;
#endif

    // The display of the window as the last layout of the window described it.
    static Display& windowDisplay;
};

} // namespace haylen::platform
#endif
