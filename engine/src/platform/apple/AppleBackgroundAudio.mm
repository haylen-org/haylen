#import "platform/apple/AppleBackgroundAudio.hpp"

#if !TARGET_OS_OSX
#import "platform/apple/HaylenRequirements+Runtime.h"

namespace haylen::platform {

bool AppleBackgroundAudio::enabled = false;
NSTimer* AppleBackgroundAudio::timer = nil;

// An app that restarts in the background without background audio stops the frames that the app before it ran there.
void AppleBackgroundAudio::setEnabled(bool value) {
    enabled = value;
    if (!value) {
        enterForeground();
    }
#if !TARGET_OS_MACCATALYST
    if (value) {
        static HaylenRequirements* const requirements = [[HaylenRequirements alloc] initWithOwner:@"The audio that plays in the background"];
        [requirements require:@[ [HaylenRequirement backgroundMode:@"audio"] ] error:nil];
    }
#endif
}

void AppleBackgroundAudio::enterBackground(void (^frame)()) {
    if (!enabled || timer != nil) {
        return;
    }
    timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / kFramesPerSecond repeats:YES block:^(NSTimer*) { frame(); }];
}

void AppleBackgroundAudio::enterForeground() {
    [timer invalidate];
    timer = nil;
}

} // namespace haylen::platform
#endif
