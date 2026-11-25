#import "platform/sdl/apple/HaylenFramePacer.h"

#if TARGET_OS_OSX
#import <QuartzCore/CADisplayLink.h>

#include <algorithm>

#include <SDL3/SDL_events.h>

#include "platform/NativeWindow.hpp"

using haylen::platform::NativeWindow;

@implementation HaylenFramePacer {
    CADisplayLink* displayLink;
    NSTimer* timer;
    NSScreen* screen;
    std::uint32_t wakeType;
    int rate;
    int hiddenRate;
    BOOL hidden;
    BOOL running;
    // Whether a frame came that the runtime has not taken yet, and whether the event that wakes the main callbacks waits in the queue of SDL, so a frame that comes before the runtime received it adds no other.
    BOOL frameDue;
    BOOL wakePending;
}

+ (HaylenFramePacer*)shared {
    static HaylenFramePacer* pacer = [HaylenFramePacer new];
    return pacer;
}

- (void)startWithWakeType:(std::uint32_t)type {
    wakeType = type;
    running = YES;
    [self restart];
}

- (void)paceAtRate:(int)value hiddenRate:(int)hiddenValue hidden:(BOOL)isHidden {
    if (value == rate && hiddenValue == hiddenRate && isHidden == hidden) {
        return;
    }

    rate = value;
    hiddenRate = hiddenValue;
    hidden = isHidden;
    [self restart];
}

- (void)followScreen {
    if ([self currentScreen] != screen) {
        [self restart];
    }
}

- (void)receiveWake {
    wakePending = NO;
}

- (BOOL)takeFrame {
    const BOOL due = frameDue;
    frameDue = NO;
    return due;
}

- (void)stop {
    running = NO;
    [self invalidate];
}

// The screen of the window, or the main screen before the window opens. A window that stays hidden until its first complete frame lies on a screen already.
- (NSScreen*)currentScreen {
    NSWindow* window = (__bridge NSWindow*)NativeWindow::getHandle();
    return window != nil ? window.screen : NSScreen.mainScreen;
}

// The display link asks the screen for the rate of the app, or for the highest refresh of the screen without one, and both sources tick in every mode of the run loop, so the frames that SDL plays during a live resize come at the refreshes too.
- (void)restart {
    [self invalidate];
    if (!running) {
        return;
    }

    screen = [self currentScreen];
    if (hidden || screen == nil) {
        // clang-format off
        timer = [NSTimer timerWithTimeInterval:1.0 / hiddenRate repeats:YES block:^(NSTimer*) {
            [self arrive];
        }];
        // clang-format on
        [NSRunLoop.mainRunLoop addTimer:timer forMode:NSRunLoopCommonModes];
        return;
    }

    const auto maximum = static_cast<float>(screen.maximumFramesPerSecond);
    const float preferred = rate > 0 ? std::min(static_cast<float>(rate), maximum) : maximum;
    displayLink = [screen displayLinkWithTarget:self selector:@selector(refresh:)];
    displayLink.preferredFrameRateRange = CAFrameRateRangeMake(preferred, preferred, preferred);
    [displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

- (void)invalidate {
    [displayLink invalidate];
    displayLink = nil;
    [timer invalidate];
    timer = nil;
    screen = nil;
}

- (void)refresh:(CADisplayLink*)sender {
    [self arrive];
}

// An event that SDL cannot queue leaves the wake to the next frame.
- (void)arrive {
    frameDue = YES;
    if (wakePending) {
        return;
    }

    SDL_Event event{};
    event.type = wakeType;
    wakePending = SDL_PushEvent(&event) ? YES : NO;
}

@end
#endif
