#import "platform/apple/HaylenViewController.h"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import "platform/apple/AppleStatusBar.hpp"

using haylen::platform::AppleStatusBar;
using haylen::platform::Window;

@implementation HaylenViewController

- (BOOL)prefersStatusBarHidden {
    return AppleStatusBar::get() == Window::StatusBar::Hidden;
}

- (UIStatusBarStyle)preferredStatusBarStyle {
    return AppleStatusBar::getStyle();
}

@end
#endif
