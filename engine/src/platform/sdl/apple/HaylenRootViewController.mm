#import "platform/sdl/apple/HaylenRootViewController.h"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import "platform/apple/AppleCutouts.hpp"
#import "platform/apple/AppleFold.hpp"
#import "platform/apple/AppleOrientation.hpp"
#import "platform/apple/AppleStatusBar.hpp"
#import "platform/apple/AppleSystemGestures.hpp"

using haylen::platform::AppleCutouts;
using haylen::platform::AppleFold;
using haylen::platform::AppleOrientation;
using haylen::platform::AppleStatusBar;
using haylen::platform::AppleSystemGestures;
using haylen::platform::Window;

@implementation HaylenRootViewController

// The fold of a foldable iPhone and the cutouts of the screen move with every layout of the window, such as when the device turns or folds.
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    AppleFold::attach(self.view);
    AppleFold::refresh(self.view);
    AppleCutouts::refresh(self.view);
}

- (BOOL)prefersStatusBarHidden {
    return AppleStatusBar::get() == Window::StatusBar::Hidden;
}

- (UIStatusBarStyle)preferredStatusBarStyle {
    return AppleStatusBar::getStyle();
}

- (BOOL)prefersHomeIndicatorAutoHidden {
    return AppleSystemGestures::isHomeIndicatorAutoHidden();
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
    return AppleSystemGestures::getDeferredEdges();
}

// The view controller allows the orientations that the application delegate allows, so UIKit never lays the window out in an orientation the app refuses, such as for the lock screen or a snapshot of the app in the background.
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return AppleOrientation::getSupported();
}

@end
#endif
