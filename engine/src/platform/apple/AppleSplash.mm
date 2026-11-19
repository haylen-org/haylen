#import "platform/apple/AppleSplash.hpp"

#if !TARGET_OS_OSX
namespace haylen::platform {

UIViewController* AppleSplash::launch = nil;

// The system of visionOS shows no launch screen, so nothing covers its window.
void AppleSplash::cover(UIViewController* root) {
#if TARGET_OS_VISION
    static_cast<void>(root);
#else
    NSString* name = [NSBundle.mainBundle objectForInfoDictionaryKey:@"UILaunchStoryboardName"];
    if (name == nil || [NSBundle.mainBundle pathForResource:name ofType:@"storyboardc"] == nil) {
        return;
    }

    launch = [[UIStoryboard storyboardWithName:name bundle:nil] instantiateInitialViewController];
    [root addChildViewController:launch];

    UIView* view = launch.view;
    view.frame = root.view.bounds;
    view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [root.view addSubview:view];
    [launch didMoveToParentViewController:root];
#endif
}

void AppleSplash::end(float fadeOutSeconds) {
    UIViewController* cover = launch;
    launch = nil;

    if (cover == nil) {
        return;
    }

    // clang-format off
    [UIView animateWithDuration:fadeOutSeconds animations:^{
        cover.view.alpha = 0.0;
    } completion:^(BOOL) {
        [cover willMoveToParentViewController:nil];
        [cover.view removeFromSuperview];
        [cover removeFromParentViewController];
    }];
    // clang-format on
}

} // namespace haylen::platform
#endif
