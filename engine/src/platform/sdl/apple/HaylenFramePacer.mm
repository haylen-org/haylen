#import "platform/sdl/apple/HaylenFramePacer.h"

#import <QuartzCore/CADisplayLink.h>
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include <algorithm>

#include <SDL3/SDL_events.h>

#include "haylen/gpu/Device.hpp"
#include "platform/NativeWindow.hpp"

using haylen::platform::NativeWindow;

@implementation HaylenFramePacer {
    CADisplayLink* displayLink;
    NSTimer* timer;
#if TARGET_OS_OSX
    NSScreen* screen;
#endif
    haylen::gpu::Device* device;
    std::uint32_t wakeType;
    int rate;
    int hiddenRate;
    BOOL hidden;
    BOOL running;
    // Whether the device paces the window now, which then stops when the pacer changes how it paces.
    BOOL devicePaces;
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

// A device that went away took its pacing with it, so the pacer asks only the new one.
- (void)followDevice:(haylen::gpu::Device*)value {
    devicePaces = NO;
    device = value;
    [self restart];
}

- (void)followScreen {
#if TARGET_OS_OSX
    if ([self currentScreen] != screen) {
        [self restart];
    }
#endif
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

#if TARGET_OS_OSX
// The screen of the window, or the main screen before the window opens. A window that stays hidden until its first complete frame lies on a screen already.
- (NSScreen*)currentScreen {
    NSWindow* window = (__bridge NSWindow*)NativeWindow::getHandle();
    return window != nil ? window.screen : NSScreen.mainScreen;
}
#endif

// The highest refresh of the screen of the window, or 0 where the system names none, such as on visionOS and before the window of UIKit opens, whose display links then run at the refresh that the system picks.
- (int)maximumRate {
#if TARGET_OS_OSX
    return screen != nil ? static_cast<int>(screen.maximumFramesPerSecond) : 0;
#elif TARGET_OS_VISION
    return 0;
#else
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    return window != nil ? static_cast<int>(window.windowScene.screen.maximumFramesPerSecond) : 0;
#endif
}

// The display links ask for the rate of the app, or for the highest refresh of the screen without one. A macOS window that shows nothing takes its frames from the timer, which ticks in every mode of the run loop like the display links, so the frames that SDL plays during a live resize come at the refreshes too.
- (void)restart {
    [self invalidate];
    if (!running) {
        return;
    }

#if TARGET_OS_OSX
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
#endif

    const int maximum = [self maximumRate];
    const int preferred = rate > 0 && maximum > 0 ? std::min(rate, maximum) : std::max(rate, maximum);
    devicePaces = device != nullptr && device->paceWindow(preferred, [] { [HaylenFramePacer.shared arrive]; }) ? YES : NO;
    if (devicePaces) {
        return;
    }

#if TARGET_OS_OSX
    displayLink = [screen displayLinkWithTarget:self selector:@selector(refresh:)];
#else
    displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(refresh:)];
#endif
    const auto range = static_cast<float>(preferred);
    displayLink.preferredFrameRateRange = preferred > 0 ? CAFrameRateRangeMake(range, range, range) : CAFrameRateRangeDefault;
    [displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

- (void)invalidate {
    [displayLink invalidate];
    displayLink = nil;
    [timer invalidate];
    timer = nil;
#if TARGET_OS_OSX
    screen = nil;
#endif

    if (devicePaces) {
        devicePaces = NO;
        static_cast<void>(device->paceWindow(0, {}));
    }
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
