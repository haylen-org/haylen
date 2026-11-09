#import "platform/apple/ApplePresenter.hpp"

#if !TARGET_OS_OSX
#include "platform/NativeWindow.hpp"

namespace haylen::platform {

UIViewController* ApplePresenter::getTopmost() {
    if (!NativeWindow::isOpen()) {
        return nil;
    }
    UIViewController* top = ((__bridge UIWindow*)NativeWindow::getHandle()).rootViewController;
    while (top.presentedViewController != nil && !top.presentedViewController.isBeingDismissed) {
        top = top.presentedViewController;
    }
    return top;
}

} // namespace haylen::platform
#endif
