#pragma once

#import <QuartzCore/CAMetalDisplayLink.h>

#include <functional>

// The delegate of the display link of the layer of a window, which hands each drawable that the display link offers to the swapchain of the window on the main run loop.
API_AVAILABLE(macos(14.0), ios(17.0), tvos(17.0))
@interface HaylenDisplayLinkDelegate : NSObject <CAMetalDisplayLinkDelegate>

- (instancetype)initWithUpdate:(std::function<void(id<CAMetalDrawable>)>)update;

@end
