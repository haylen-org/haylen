#import "gpu/apple/HaylenGpuTestSceneDelegate.h"

#include "gpu/apple/GpuTestApp.hpp"

@implementation HaylenGpuTestSceneDelegate

// The tests take no input for minutes, so the app keeps the idle timer from starting the screen saver of tvOS or locking an iPhone, which would send the app to the background.
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions {
    UIApplication.sharedApplication.idleTimerDisabled = YES;
    self.window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene*)scene];
    self.window.rootViewController = [[UIViewController alloc] init];
    [self.window makeKeyAndVisible];
}

// The tests start on the next turn of the main loop, after the system has shown the scene, and keep the main thread until the app quits.
- (void)sceneDidBecomeActive:(UIScene*)scene {
    dispatch_async(dispatch_get_main_queue(), ^{ haylen::test::GpuTestApp::runTests(); });
}

@end
