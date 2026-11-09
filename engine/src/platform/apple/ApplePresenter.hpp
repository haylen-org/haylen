#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

namespace haylen::platform {

// Finds where native UI presents over the app on iOS, iPadOS, tvOS and Mac Catalyst: the topmost view controller that the window of the app presents, so it works while other UI, such as the screen of a plugin or an alert, already shows.
class ApplePresenter final {
  public:
    // The topmost view controller that is not being dismissed, or `nil` while the app has no window.
    [[nodiscard]] static UIViewController* getTopmost();
};

} // namespace haylen::platform
#endif
