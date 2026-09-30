#import "platform/apple/HaylenThemeView.h"

#if !TARGET_OS_OSX
#import "platform/apple/AppleTheme.hpp"

using haylen::platform::AppleTheme;

@implementation HaylenThemeView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
    [super traitCollectionDidChange:previousTraitCollection];
    if (previousTraitCollection == nil || previousTraitCollection.userInterfaceStyle != self.traitCollection.userInterfaceStyle) {
        AppleTheme::report(self.traitCollection);
    }
}

@end
#endif
