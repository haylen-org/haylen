#import "platform/sdl/apple/HaylenRootViewController.h"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import "platform/apple/AppleOrientation.hpp"
#import "platform/apple/AppleStatusBar.hpp"

using haylen::platform::AppleOrientation;
using haylen::platform::AppleStatusBar;
using haylen::platform::Window;

@implementation HaylenRootViewController

- (BOOL)prefersStatusBarHidden {
    return AppleStatusBar::get() == Window::StatusBar::Hidden;
}

- (UIStatusBarStyle)preferredStatusBarStyle {
    return AppleStatusBar::getStyle();
}

// The view controller allows the orientations that the application delegate allows, so UIKit never lays the window out in an orientation the app refuses, such as for the lock screen or a snapshot of the app in the background.
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return AppleOrientation::getSupported();
}

@end
#endif
