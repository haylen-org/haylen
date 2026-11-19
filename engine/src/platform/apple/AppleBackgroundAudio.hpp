#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <Foundation/Foundation.h>

#include "haylen/port/BackgroundAudioService.hpp"

namespace haylen::platform {

// Keeps the frames of a UIKit app that plays audio in the background running there. The display link of UIKit stops in the background, so a timer of the main run loop runs the frames, which refill the streams of the mixer. UIKit runs it only while the audio session plays, and suspends the app once its sound stops. On iOS, iPadOS, tvOS and visionOS the app declares that it plays there with the mode `audio` of `UIBackgroundModes` in the `Info.plist` of its bundle, while a Mac keeps every Mac Catalyst app running in the background.
class AppleBackgroundAudio final : public port::BackgroundAudioService {
  public:
    [[nodiscard]] bool isDeclared() override;

    // Takes whether the app that starts plays in the background, and turns it on or off while the app runs.
    void setEnabled(bool value) override;

    // Runs the frame from now on at `kFramesPerSecond` while the app plays in the background, until the app comes back.
    static void enterBackground(void (^frame)());
    static void enterForeground();

  private:
    static constexpr double kFramesPerSecond = 10.0;

    [[nodiscard]] static bool hasAudioMode();

    static bool enabled;
    static NSTimer* timer;
};

} // namespace haylen::platform
#endif
