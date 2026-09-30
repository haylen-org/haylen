#import "platform/apple/ApplePresenter.hpp"

#if !TARGET_OS_OSX
#include "sokol_app.h"

namespace haylen::platform {

UIViewController* ApplePresenter::getTopmost() {
    if (!sapp_isvalid()) {
        return nil;
    }
    UIViewController* top = ((__bridge UIWindow*)sapp_ios_get_window()).rootViewController;
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
