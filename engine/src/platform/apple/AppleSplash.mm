#import "platform/apple/AppleSplash.hpp"

#if !TARGET_OS_OSX
namespace haylen::platform {

UIView* AppleSplash::view = nil;

void AppleSplash::cover(UIWindow* window) {
    NSString* name = [NSBundle.mainBundle objectForInfoDictionaryKey:@"UILaunchStoryboardName"];
    if (name == nil || [NSBundle.mainBundle pathForResource:name ofType:@"storyboardc"] == nil) {
        return;
    }
    UIViewController* launch = [[UIStoryboard storyboardWithName:name bundle:nil] instantiateInitialViewController];
    view = launch.view;
    view.frame = window.bounds;
    view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [window addSubview:view];
}

void AppleSplash::end(float fadeOutSeconds) {
    UIView* cover = view;
    view = nil;
    if (cover == nil) {
        return;
    }
    // clang-format off
    [UIView animateWithDuration:fadeOutSeconds animations:^{
        cover.alpha = 0.0;
    } completion:^(BOOL) {
        [cover removeFromSuperview];
    }];
    // clang-format on
}

} // namespace haylen::platform
#endif
