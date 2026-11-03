#import "platform/apple/AppleSystemGestures.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#include "platform/NativeWindow.hpp"

namespace haylen::platform {

Window::SystemGestures AppleSystemGestures::current = Window::SystemGestures::Deferred;

void AppleSystemGestures::set(Window::SystemGestures value) {
    current = value;
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    [window.rootViewController setNeedsUpdateOfHomeIndicatorAutoHidden];
    [window.rootViewController setNeedsUpdateOfScreenEdgesDeferringSystemGestures];
}

bool AppleSystemGestures::isHomeIndicatorAutoHidden() {
    return current == Window::SystemGestures::AutoHideIndicator;
}

// UIKit lets the first swipe from a deferred edge reach the app and acts only on a second swipe, which covers the home indicator at the bottom and the notification and control centers at the top.
UIRectEdge AppleSystemGestures::getDeferredEdges() {
    return current == Window::SystemGestures::Deferred ? UIRectEdgeAll : UIRectEdgeNone;
}

} // namespace haylen::platform
#endif
