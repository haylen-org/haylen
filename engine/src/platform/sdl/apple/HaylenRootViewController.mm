#import "platform/sdl/apple/HaylenRootViewController.h"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import "platform/apple/AppleStatusBar.hpp"

using haylen::platform::AppleStatusBar;
using haylen::platform::Window;

@implementation HaylenRootViewController

- (BOOL)prefersStatusBarHidden {
    return AppleStatusBar::get() == Window::StatusBar::Hidden;
}

- (UIStatusBarStyle)preferredStatusBarStyle {
    return AppleStatusBar::getStyle();
}

@end
#endif
