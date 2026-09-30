#import "platform/apple/HaylenScreenSceneDelegate.h"

#if !TARGET_OS_OSX && !TARGET_OS_TV
#import "platform/apple/AppleScreens.hpp"
#import "platform/apple/HaylenScreen+Runtime.h"

using haylen::platform::AppleScreens;

@implementation HaylenScreenSceneDelegate {
    HaylenScreen* screen;
}

- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions {
    for (NSUserActivity* activity in connectionOptions.userActivities) {
        if ([activity.activityType isEqualToString:AppleScreens::kWindowActivity]) {
            screen = AppleScreens::find([activity.userInfo[@"screen"] unsignedLongLongValue]);
        }
    }
    if (screen == nil) {
        [UIApplication.sharedApplication requestSceneSessionDestruction:session options:nil errorHandler:nil];
        return;
    }
    self.window = [screen windowForScene:static_cast<UIWindowScene*>(scene)];
    [self.window makeKeyAndVisible];
}

- (void)sceneDidDisconnect:(UIScene*)scene {
    [screen dismissed];
    screen = nil;
    self.window = nil;
    [UIApplication.sharedApplication requestSceneSessionDestruction:scene.session options:nil errorHandler:nil];
}

@end
#endif
