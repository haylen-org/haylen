#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

#include "haylen/platform/Window.hpp"

namespace haylen::platform {

// The system gestures at the edges of an iPhone or iPad, which `HaylenRootViewController` hands UIKit as the app asks. Until the engine applies the value of `app.json`, the edges defer the gestures, which is the default of `app.json` too.
class AppleSystemGestures final {
  public:
    static void set(Window::SystemGestures value);
    [[nodiscard]] static bool isHomeIndicatorAutoHidden();
    [[nodiscard]] static UIRectEdge getDeferredEdges();

  private:
    static Window::SystemGestures current;
};

} // namespace haylen::platform
#endif
