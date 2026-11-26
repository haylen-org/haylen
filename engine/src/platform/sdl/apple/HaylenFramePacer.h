#pragma once

#import <Foundation/Foundation.h>

#include <cstdint>

namespace haylen::gpu {
class Device;
}

// The source of the frames of an Apple app on the main run loop, which `platform::sdl::AppleFramePacer` drives. While the window shows, a frame comes when the GPU device offers the next drawable of the window, or at a refresh of a display link of the pacer where the device paces nothing, and while a macOS window shows nothing or no screen shows it, which no display paces, a frame comes from a timer. Each frame wakes the wait of the main callbacks of SDL with one event, which the runtime receives before the next one.
@interface HaylenFramePacer : NSObject

@property(class, nonatomic, readonly) HaylenFramePacer* shared;

- (void)startWithWakeType:(std::uint32_t)wakeType;
- (void)paceAtRate:(int)rate hiddenRate:(int)hiddenRate hidden:(BOOL)hidden;
- (void)followDevice:(haylen::gpu::Device*)device;
- (void)followScreen;
- (void)receiveWake;
- (BOOL)takeFrame;
- (void)stop;

@end
