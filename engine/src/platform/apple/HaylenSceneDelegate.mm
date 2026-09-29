#import "platform/apple/HaylenSceneDelegate.h"

#if !TARGET_OS_OSX
#import "platform/apple/ApplePlugins.hpp"
#import "platform/apple/HaylenOverlayLayer.h"
#include "sokol_app.h"

using haylen::platform::ApplePlugins;

@implementation HaylenSceneDelegate

// Plugins load before launching ends, so their SDKs set up in time, and the notification center gets its delegate as early, so the notification that launched the app reaches the plugins.
- (BOOL)application:(UIApplication*)application willFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id>*)launchOptions {
    ApplePlugins::load();
    if (!ApplePlugins::getIds().empty()) {
        UNUserNotificationCenter.currentNotificationCenter.delegate = self;
    }
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application willFinishLaunchingWithOptions:launchOptions];
    }
    return YES;
}

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id>*)launchOptions {
    const BOOL launched = [super application:application didFinishLaunchingWithOptions:launchOptions];
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application didFinishLaunchingWithOptions:launchOptions];
    }
    return launched;
}

// sokol_app creates the window here, which the overlay lies over from now on. UIKit hands the links, the user activities and the shortcut item that open the app only to the connection options, so the plugins then receive them as they would while the app runs.
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions {
    [super scene:scene willConnectToSession:session options:connectionOptions];
    [HaylenOverlayLayer.shared attachToView:((__bridge UIWindow*)sapp_ios_get_window()).rootViewController.view];
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin scene:scene willConnectToSession:session options:connectionOptions];
    }

    if (connectionOptions.URLContexts.count > 0) {
        [self scene:scene openURLContexts:connectionOptions.URLContexts];
    }
    for (NSUserActivity* activity in connectionOptions.userActivities) {
        [self scene:scene continueUserActivity:activity];
    }
#if !TARGET_OS_TV
    if (connectionOptions.shortcutItem != nil) {
        [self windowScene:static_cast<UIWindowScene*>(scene) performActionForShortcutItem:connectionOptions.shortcutItem completionHandler:^(BOOL){}];
    }
#endif
}

- (void)sceneDidDisconnect:(UIScene*)scene {
    [HaylenOverlayLayer.shared detach];
    ApplePlugins::closeCovers();
}

- (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)URLContexts {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin scene:scene openURLContexts:URLContexts];
    }
}

- (void)scene:(UIScene*)scene continueUserActivity:(NSUserActivity*)userActivity {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin scene:scene continueUserActivity:userActivity];
    }
}

#if !TARGET_OS_TV
// The action counts as handled when any plugin handled it.
- (void)windowScene:(UIWindowScene*)windowScene performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem completionHandler:(void (^)(BOOL))completionHandler {
    // clang-format off
    ApplePlugins::join(_cmd, ^(NSUInteger handled) { completionHandler(handled != 0); }, ^(id<HaylenPlugin> plugin, ApplePlugins::Answer answer) {
        [plugin windowScene:windowScene performActionForShortcutItem:shortcutItem completionHandler:^(BOOL succeeded) { answer(succeeded ? 1 : 0); }];
    });
    // clang-format on
}
#endif

- (void)sceneDidBecomeActive:(UIScene*)scene {
    [super sceneDidBecomeActive:scene];
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin sceneDidBecomeActive:scene];
    }
}

- (void)sceneWillResignActive:(UIScene*)scene {
    [super sceneWillResignActive:scene];
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin sceneWillResignActive:scene];
    }
}

- (void)sceneWillEnterForeground:(UIScene*)scene {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin sceneWillEnterForeground:scene];
    }
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin sceneDidEnterBackground:scene];
    }
}

- (void)application:(UIApplication*)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData*)deviceToken {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application didRegisterForRemoteNotificationsWithDeviceToken:deviceToken];
    }
}

- (void)application:(UIApplication*)application didFailToRegisterForRemoteNotificationsWithError:(NSError*)error {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application didFailToRegisterForRemoteNotificationsWithError:error];
    }
}

// Each plugin answers with its result as a bit, and new data outweighs a failure, which outweighs no data.
- (void)application:(UIApplication*)application didReceiveRemoteNotification:(NSDictionary*)userInfo fetchCompletionHandler:(void (^)(UIBackgroundFetchResult))completionHandler {
    // clang-format off
    ApplePlugins::Done done = ^(NSUInteger results) {
        if ((results & (1UL << UIBackgroundFetchResultNewData)) != 0) {
            completionHandler(UIBackgroundFetchResultNewData);
        } else if ((results & (1UL << UIBackgroundFetchResultFailed)) != 0) {
            completionHandler(UIBackgroundFetchResultFailed);
        } else {
            completionHandler(UIBackgroundFetchResultNoData);
        }
    };
    ApplePlugins::join(_cmd, done, ^(id<HaylenPlugin> plugin, ApplePlugins::Answer answer) {
        [plugin application:application didReceiveRemoteNotification:userInfo fetchCompletionHandler:^(UIBackgroundFetchResult result) { answer(1UL << result); }];
    });
    // clang-format on
}

- (void)application:(UIApplication*)application handleEventsForBackgroundURLSession:(NSString*)identifier completionHandler:(void (^)(void))completionHandler {
    // clang-format off
    ApplePlugins::join(_cmd, ^(NSUInteger) { completionHandler(); }, ^(id<HaylenPlugin> plugin, ApplePlugins::Answer answer) {
        [plugin application:application handleEventsForBackgroundURLSession:identifier completionHandler:^{ answer(0); }];
    });
    // clang-format on
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center willPresentNotification:(UNNotification*)notification withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler {
    ApplePlugins::presentNotification(center, notification, completionHandler);
}

#if !TARGET_OS_TV
- (void)userNotificationCenter:(UNUserNotificationCenter*)center didReceiveNotificationResponse:(UNNotificationResponse*)response withCompletionHandler:(void (^)(void))completionHandler {
    ApplePlugins::receiveNotificationResponse(center, response, completionHandler);
}
#endif

@end
#endif
