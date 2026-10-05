#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

namespace haylen::platform {

// Keeps the launch screen of an iOS, iPadOS, tvOS or Mac Catalyst app over its window until the runtime ends the splash, so the launch screen stays as long as the splash of app.json asks and nothing black shows before the first frame. The system takes its own copy away as soon as the window shows, so the runtime covers the window with the launch storyboard that the Info.plist names.
class AppleSplash final {
  public:
    static void cover(UIWindow* window);

    // Fades the cover out over the seconds and removes it. It runs on the main thread, which draws the frames of the app.
    static void end(float fadeOutSeconds);

  private:
    static UIView* view;
};

} // namespace haylen::platform
#endif
