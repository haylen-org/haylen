#import "platform/apple/AppleStatusBar.hpp"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#include "sokol_app.h"

namespace haylen::platform {

std::optional<Window::StatusBar> AppleStatusBar::current;

Window::StatusBar AppleStatusBar::get() {
    if (!current) {
        current = fromInfoPlist();
    }
    return *current;
}

void AppleStatusBar::set(Window::StatusBar value) {
    current = value;
    UIWindow* window = (__bridge UIWindow*)sapp_ios_get_window();
    [window.rootViewController setNeedsStatusBarAppearanceUpdate];
}

UIStatusBarStyle AppleStatusBar::getStyle() {
    switch (get()) {
    case Window::StatusBar::Light:
        return UIStatusBarStyleLightContent;
    case Window::StatusBar::Dark:
        return UIStatusBarStyleDarkContent;
    case Window::StatusBar::Hidden:
    case Window::StatusBar::Auto:
        return UIStatusBarStyleDefault;
    }
    return UIStatusBarStyleDefault;
}

float AppleStatusBar::getHeight() {
    UIWindow* window = (__bridge UIWindow*)sapp_ios_get_window();
    UIStatusBarManager* manager = window.windowScene.statusBarManager;
    if (manager == nil || manager.isStatusBarHidden) {
        return 0.0F;
    }
    return static_cast<float>(manager.statusBarFrame.size.height) * sapp_dpi_scale();
}

// The keys that haylen.py writes from `app.json` launch the app with its status bar, so the view controller keeps it until the engine applies the same value.
Window::StatusBar AppleStatusBar::fromInfoPlist() {
    NSDictionary* info = NSBundle.mainBundle.infoDictionary;
    if ([info[@"UIStatusBarHidden"] boolValue]) {
        return Window::StatusBar::Hidden;
    }
    NSString* style = info[@"UIStatusBarStyle"];
    if ([style isEqualToString:@"UIStatusBarStyleLightContent"]) {
        return Window::StatusBar::Light;
    }
    if ([style isEqualToString:@"UIStatusBarStyleDarkContent"]) {
        return Window::StatusBar::Dark;
    }
    return Window::StatusBar::Auto;
}

} // namespace haylen::platform
#endif
