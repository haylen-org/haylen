#import "platform/apple/HaylenScrollGesture.h"

#if !TARGET_OS_OSX && !TARGET_OS_TV
#include <utility>

using haylen::platform::Event;

@implementation HaylenScrollGesture {
    std::function<void(const Event&)> deliver;
}

// The gesture takes no finger, so it reads only the scrolls of a pointer and never holds a touch from the view of the app.
- (instancetype)initWithDelivery:(std::function<void(const Event&)>)value {
    self = [super initWithTarget:nil action:nil];
    deliver = std::move(value);
    self.allowedScrollTypesMask = UIScrollTypeMaskAll;
    self.allowedTouchTypes = @[];
    self.cancelsTouchesInView = NO;
    [self addTarget:self action:@selector(scrolled)];
    return self;
}

- (BOOL)isPointerLocked {
    UIPointerLockState* state = self.view.window.windowScene.pointerLockState;
    return state != nil && state.isLocked;
}

- (Event::ScrollPhase)phase {
    switch (self.state) {
    case UIGestureRecognizerStateBegan:
        return Event::ScrollPhase::Began;
    case UIGestureRecognizerStateChanged:
        return Event::ScrollPhase::Changed;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
        return Event::ScrollPhase::Ended;
    default:
        return Event::ScrollPhase::None;
    }
}

// Each report moves by the points that the pan travelled since the one before, and the gesture that begins or ends reaches the app even when it did not move, as the fingers touch or leave a trackpad.
- (void)scrolled {
    if ([self isPointerLocked]) {
        return;
    }

    const CGPoint moved = [self translationInView:self.view];
    [self setTranslation:CGPointZero inView:self.view];
    const Event::ScrollPhase phase = [self phase];
    const bool still = moved.x == 0.0 && moved.y == 0.0;
    if (still && phase != Event::ScrollPhase::Began && phase != Event::ScrollPhase::Ended) {
        return;
    }

    const UIKeyModifierFlags flags = self.modifierFlags;
    const haylen::input::KeyModifiers modifiers{.shift = (flags & UIKeyModifierShift) != 0, .control = (flags & UIKeyModifierControl) != 0, .alt = (flags & UIKeyModifierAlternate) != 0, .super = (flags & UIKeyModifierCommand) != 0};
    const haylen::math::Vec2 scroll{static_cast<float>(moved.x), static_cast<float>(moved.y)};
    deliver({.type = Event::Type::MouseScroll, .modifiers = modifiers, .scroll = scroll, .preciseScroll = true, .scrollPhase = phase});
}

@end
#endif
