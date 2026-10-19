#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

#include <optional>

#include "haylen/platform/Window.hpp"

namespace haylen::platform {

// The status bar of an iPhone or iPad, which `HaylenViewController` shows as the app asks. Until the app sets it, the status bar stays as the Info.plist launched it from `app.json`.
class AppleStatusBar final {
  public:
    [[nodiscard]] static Window::StatusBar get();
    static void set(Window::StatusBar value);
    [[nodiscard]] static UIStatusBarStyle getStyle();

    // The height in framebuffer pixels of the status bar that shows, which UIKit also hides in landscape on an iPhone.
    [[nodiscard]] static float getHeight();

  private:
    static std::optional<Window::StatusBar> current;

    [[nodiscard]] static Window::StatusBar fromInfoPlist();
};

} // namespace haylen::platform
#endif
