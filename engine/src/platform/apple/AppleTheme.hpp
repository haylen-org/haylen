#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

namespace haylen::platform {

// Reports the theme of the system to the engine: the effective appearance of the app on macOS, which the application delegate follows with key-value observing, and the trait collection of the window of the app on iOS, iPadOS, tvOS and Mac Catalyst, which a view inside the window follows.
class AppleTheme final {
  public:
#if TARGET_OS_OSX
    static void report(NSAppearance* appearance);
#else
    static void report(UITraitCollection* traits);

    // Reports the theme of the window now and after every change of its traits.
    static void observe(UIWindow* window);
#endif
};

} // namespace haylen::platform
