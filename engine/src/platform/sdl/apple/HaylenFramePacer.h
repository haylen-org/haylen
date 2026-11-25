#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>

#include <cstdint>

// The source of the frames of a macOS app on the main run loop, which `platform::sdl::AppleFramePacer` drives. While the window shows, a frame comes at a refresh of the display link of the screen of the window, and while the window shows nothing or no screen shows it, which no display paces, a frame comes from a timer. Each frame wakes the wait of the main callbacks of SDL with one event, which the runtime receives before the next one.
@interface HaylenFramePacer : NSObject

@property(class, nonatomic, readonly) HaylenFramePacer* shared;

- (void)startWithWakeType:(std::uint32_t)wakeType;
- (void)paceAtRate:(int)rate hiddenRate:(int)hiddenRate hidden:(BOOL)hidden;
- (void)followScreen;
- (void)receiveWake;
- (BOOL)takeFrame;
- (void)stop;

@end
#endif
