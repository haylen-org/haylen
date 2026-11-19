#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

namespace haylen::platform {

// Keeps the launch screen of an iOS, iPadOS, tvOS or Mac Catalyst app over its window until the runtime ends the splash, so the launch screen stays as long as the splash of haylen.json asks and nothing shows before the first complete frame. The system takes its own copy away as soon as the window shows, so the runtime covers the view of the app with the launch storyboard that the Info.plist names. The cover is a child of the root view controller whose view lies inside the view of the app, since UIKit puts the view of the root view controller above every view of the window when the window first shows. The storyboard centers its logo in the whole view, so the logo keeps the place that the launch screen of the system gave it, whatever safe area the app has.
class AppleSplash final {
  public:
    static void cover(UIViewController* root);

    // Fades the cover out over the seconds and removes it. It runs on the main thread, which draws the frames of the app.
    static void end(float fadeOutSeconds);

  private:
    static UIViewController* launch;
};

} // namespace haylen::platform
#endif
