#import "platform/apple/AppleDisplays.hpp"

#if !TARGET_OS_OSX
#include <algorithm>
#include <utility>

#include "haylen/platform/Event.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"

namespace haylen::platform {

#if TARGET_OS_VISION
// The window of the app is its display, in its own points, with the safe area of the window as the work area. visionOS tells no rate and no modes of what shows the window.
std::vector<Display> AppleDisplays::getDisplays() {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    const CGRect bounds = window != nil ? window.bounds : CGRectZero;
    const CGRect work = window != nil ? UIEdgeInsetsInsetRect(bounds, window.safeAreaInsets) : CGRectZero;
    const float scale = window != nil ? static_cast<float>(window.traitCollection.displayScale) : 1.0F;
    return {{.id = "main", .bounds = {0.0F, 0.0F, static_cast<float>(bounds.size.width), static_cast<float>(bounds.size.height)}, .workArea = {static_cast<float>(work.origin.x), static_cast<float>(work.origin.y), static_cast<float>(work.size.width), static_cast<float>(work.size.height)}, .scale = scale, .primary = true}};
}

std::string AppleDisplays::getWindowDisplayId() {
    return "main";
}
#else
NSMapTable<UIScreen*, NSString*>* AppleDisplays::ids = [NSMapTable weakToStrongObjectsMapTable];
NSUInteger AppleDisplays::counted = 0;

NSString* AppleDisplays::getId(UIScreen* screen) {
    if (screen == UIScreen.mainScreen) {
        return @"main";
    }
    NSString* known = [ids objectForKey:screen];
    if (known == nil) {
        known = [NSString stringWithFormat:@"external-%lu", static_cast<unsigned long>(++counted)];
        [ids setObject:known forKey:screen];
    }
    return known;
}

// The work area of the screen that shows the window of the app leaves out the safe area of the window, and the one of another screen the edges that a TV may crop, which UIKit compensates for. UIKit gives screens no names, and each mode of a screen is a size of its pixels at the rate that the screen shows.
Display AppleDisplays::describe(UIScreen* screen) {
    const CGRect bounds = screen.bounds;
    UIEdgeInsets insets = UIEdgeInsetsZero;
#if !TARGET_OS_TV
    insets = screen.overscanCompensationInsets;
#endif
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    if (window != nil && window.windowScene.screen == screen) {
        insets = window.safeAreaInsets;
    }
    const CGRect work = UIEdgeInsetsInsetRect(bounds, insets);
    const auto rate = static_cast<float>(screen.maximumFramesPerSecond);
    const auto scale = static_cast<float>(screen.scale);

    std::vector<Display::Mode> modes;
#if !TARGET_OS_TV
    for (UIScreenMode* mode in screen.availableModes) {
        const Display::Mode found{.width = static_cast<int>(mode.size.width), .height = static_cast<int>(mode.size.height), .refreshRate = rate, .scale = scale};
        if (std::ranges::find(modes, found) == modes.end()) {
            modes.push_back(found);
        }
    }
#endif
    return {.id = getId(screen).UTF8String, .bounds = {0.0F, 0.0F, static_cast<float>(bounds.size.width), static_cast<float>(bounds.size.height)}, .workArea = {static_cast<float>(work.origin.x), static_cast<float>(work.origin.y), static_cast<float>(work.size.width), static_cast<float>(work.size.height)}, .scale = scale, .refreshRate = rate, .primary = screen == UIScreen.mainScreen, .modes = std::move(modes)};
}

std::vector<Display> AppleDisplays::getDisplays() {
    NSMutableArray<UIScreen*>* screens = [NSMutableArray arrayWithObject:UIScreen.mainScreen];
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:UIWindowScene.class] && ![screens containsObject:static_cast<UIWindowScene*>(scene).screen]) {
            [screens addObject:static_cast<UIWindowScene*>(scene).screen];
        }
    }

    std::vector<Display> displays;
    displays.reserve(screens.count);
    for (UIScreen* screen in screens) {
        displays.push_back(describe(screen));
    }
    return displays;
}

std::string AppleDisplays::getWindowDisplayId() {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    UIScreen* screen = window.windowScene.screen;
    return getId(screen != nil ? screen : UIScreen.mainScreen).UTF8String;
}
#endif

void AppleDisplays::changed() {
    Player::postEvent({.type = Event::Type::DisplaysChanged});
}

} // namespace haylen::platform
#endif
