#import "platform/apple/AppleBackgroundAudio.hpp"

#if !TARGET_OS_OSX
#import <AVFoundation/AVFoundation.h>

#include "haylen/core/Log.hpp"

namespace haylen::platform {

bool AppleBackgroundAudio::enabled = false;
NSTimer* AppleBackgroundAudio::timer = nil;

// The bundle stays the same while the process lives, so it is read once.
bool AppleBackgroundAudio::isDeclared() {
#if TARGET_OS_MACCATALYST
    return true;
#else
    static const bool declared = hasAudioMode();
    return declared;
#endif
}

bool AppleBackgroundAudio::hasAudioMode() {
    id modes = [NSBundle.mainBundle objectForInfoDictionaryKey:@"UIBackgroundModes"];
    return [modes isKindOfClass:NSArray.class] && [modes containsObject:@"audio"];
}

// An app that restarts in the background without background audio stops the frames that the app before it ran there.
void AppleBackgroundAudio::setEnabled(bool value) {
    enabled = value;

    if (!value) {
        enterForeground();
    }
}

// The system keeps only the playback session of the audio running in the background, which the app chooses with the "iosSession" of "haylen.json".
void AppleBackgroundAudio::enterBackground(void (^frame)()) {
    if (!enabled || timer != nil) {
        return;
    }

    static bool reported = false;
    if (!reported && ![AVAudioSession.sharedInstance.category isEqualToString:AVAudioSessionCategoryPlayback]) {
        reported = true;
        core::Log::warning("The app declares that it plays audio in the background, which the system allows only to the audio session that \"audio.iosSession\" of \"haylen.json\" sets to \"playback\".");
    }

    timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / kFramesPerSecond repeats:YES block:^(NSTimer*) { frame(); }];
}

void AppleBackgroundAudio::enterForeground() {
    [timer invalidate];
    timer = nil;
}

} // namespace haylen::platform
#endif
