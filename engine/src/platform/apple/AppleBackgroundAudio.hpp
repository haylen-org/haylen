#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <Foundation/Foundation.h>

namespace haylen::platform {

// Keeps the frames of a UIKit app that plays audio in the background running there. The display link of UIKit stops in the background, so a timer of the main run loop runs the frames, which refill the streams of the mixer and deliver the commands of the lock screen to the app. UIKit runs it only while the audio session plays, and suspends the app once its sound stops. On iOS, iPadOS, tvOS and visionOS the mode `audio` of `UIBackgroundModes` lets the app play there, which a Mac Catalyst app needs not.
class AppleBackgroundAudio final {
  public:
    // Takes whether the app that starts plays in the background, and logs once what its project lacks for it.
    static void setEnabled(bool value);

    // Runs the frame from now on at `kFramesPerSecond` while the app plays in the background, until the app comes back.
    static void enterBackground(void (^frame)());
    static void enterForeground();

  private:
    static constexpr double kFramesPerSecond = 10.0;

    static bool enabled;
    static NSTimer* timer;
};

} // namespace haylen::platform
#endif
