#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX && !TARGET_OS_TV
#import <UIKit/UIKit.h>

#include <functional>

#include "haylen/platform/Event.hpp"

// The scrolls of the wheels and trackpads over the view of an iPad, Mac Catalyst or visionOS app, which UIKit reports for every source, such as a mouse, a trackpad, Voice Control, Switch Control, remote control and UI tests, as a pan of scroll events in points of the view with the phases of the gesture. GameController reads only the mice of the hardware, so it reports the wheel only while the scene of the window locks the pointer, when UIKit reports no scroll, and the gesture reports the others.
@interface HaylenScrollGesture : UIPanGestureRecognizer

- (instancetype)initWithDelivery:(std::function<void(const haylen::platform::Event&)>)deliver;

@end
#endif
