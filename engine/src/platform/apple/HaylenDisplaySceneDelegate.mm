#import "platform/apple/HaylenDisplaySceneDelegate.h"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import "platform/apple/AppleDisplays.hpp"
#import "platform/apple/AppleOutputs.hpp"

using haylen::platform::AppleDisplays;
using haylen::platform::AppleOutputs;

@implementation HaylenDisplaySceneDelegate

- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions {
    AppleDisplays::changed();
}

- (void)sceneDidDisconnect:(UIScene*)scene {
    AppleOutputs::disconnect(scene);
    AppleDisplays::changed();
}

@end
#endif
