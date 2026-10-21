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

void ApplePresenter::present(UIViewController* controller, void (^failed)(void)) {
    UIViewController* top = getTopmost();
    if (top == nil) {
        failed();
        return;
    }
    id<UIViewControllerTransitionCoordinator> coordinator = top.transitionCoordinator;
    if (coordinator != nil) {
        [coordinator animateAlongsideTransition:nil completion:^(id<UIViewControllerTransitionCoordinatorContext>) { present(controller, failed); }];
        return;
    }
    [top presentViewController:controller animated:YES completion:nil];
}

} // namespace haylen::platform
#endif
