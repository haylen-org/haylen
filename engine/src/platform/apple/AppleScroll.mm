#import "platform/apple/AppleScroll.hpp"

#if TARGET_OS_OSX
#include <utility>

#include "platform/NativeWindow.hpp"

namespace haylen::platform {

void AppleScroll::watch(std::function<void(const Event&)> deliver) {
    // clang-format off
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel handler:^NSEvent*(NSEvent* event) {
        if (event.window != (__bridge NSWindow*)NativeWindow::getHandle()) {
            return event;
        }
        const Event scroll = translate(event);
        const bool moved = scroll.scroll.x != 0.0F || scroll.scroll.y != 0.0F;
        if (moved || scroll.scrollPhase == Event::ScrollPhase::Began || scroll.scrollPhase == Event::ScrollPhase::Ended) {
            deliver(scroll);
        }
        return event;
    }];
    // clang-format on
}

Event AppleScroll::translate(NSEvent* event) {
    const NSEventModifierFlags flags = event.modifierFlags;
    const input::KeyModifiers modifiers{.shift = (flags & NSEventModifierFlagShift) != 0, .control = (flags & NSEventModifierFlagControl) != 0, .alt = (flags & NSEventModifierFlagOption) != 0, .super = (flags & NSEventModifierFlagCommand) != 0};
    const math::Vec2 scroll{static_cast<float>(event.scrollingDeltaX), static_cast<float>(event.scrollingDeltaY)};
    return {.type = Event::Type::MouseScroll, .modifiers = modifiers, .scroll = scroll, .preciseScroll = event.hasPreciseScrollingDeltas == YES, .scrollPhase = toPhase(event)};
}

// The momentum that the system adds once the fingers left the trackpad has a phase of its own, and the fingers that touch the trackpad may begin a gesture before they move.
Event::ScrollPhase AppleScroll::toPhase(NSEvent* event) {
    if (event.momentumPhase != NSEventPhaseNone) {
        return Event::ScrollPhase::Momentum;
    }
    if ((event.phase & (NSEventPhaseBegan | NSEventPhaseMayBegin)) != 0) {
        return Event::ScrollPhase::Began;
    }
    if ((event.phase & NSEventPhaseChanged) != 0) {
        return Event::ScrollPhase::Changed;
    }
    if ((event.phase & (NSEventPhaseEnded | NSEventPhaseCancelled)) != 0) {
        return Event::ScrollPhase::Ended;
    }
    return Event::ScrollPhase::None;
}

} // namespace haylen::platform
#endif
