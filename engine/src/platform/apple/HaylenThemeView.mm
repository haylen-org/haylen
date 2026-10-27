#import "platform/apple/HaylenThemeView.h"

#if !TARGET_OS_OSX
#import "platform/apple/AppleTheme.hpp"

using haylen::platform::AppleTheme;

@implementation HaylenThemeView

#if TARGET_OS_VISION
// The system of visionOS reports changes of traits only to the handlers that a view registers for them.
- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ] withHandler:^(HaylenThemeView* view, UITraitCollection*) { AppleTheme::report(view.traitCollection); }];
    return self;
}
#else
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
    [super traitCollectionDidChange:previousTraitCollection];
    if (previousTraitCollection == nil || previousTraitCollection.userInterfaceStyle != self.traitCollection.userInterfaceStyle) {
        AppleTheme::report(self.traitCollection);
    }
}
#endif

@end
#endif
