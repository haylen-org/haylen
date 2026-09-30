#import "platform/apple/AppleTheme.hpp"

#include "haylen/platform/Theme.hpp"
#include "platform/SystemState.hpp"
#include "platform/sokol/SokolHost.hpp"

#if !TARGET_OS_OSX
#import "platform/apple/HaylenThemeView.h"
#endif

namespace haylen::platform {

#if TARGET_OS_OSX
void AppleTheme::report(NSAppearance* appearance) {
    const bool dark = [[appearance bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]] isEqualToString:NSAppearanceNameDarkAqua];
    SokolHost::getSystemState().setTheme(dark ? Theme::Dark : Theme::Light);
}
#else
void AppleTheme::report(UITraitCollection* traits) {
    SokolHost::getSystemState().setTheme(traits.userInterfaceStyle == UIUserInterfaceStyleDark ? Theme::Dark : Theme::Light);
}

// The view is hidden and never takes a touch, so it only carries the traits of the window.
void AppleTheme::observe(UIWindow* window) {
    HaylenThemeView* view = [[HaylenThemeView alloc] initWithFrame:CGRectZero];
    view.hidden = YES;
    view.userInteractionEnabled = NO;
    [window addSubview:view];
    report(window.traitCollection);
}
#endif

} // namespace haylen::platform
