#import "platform/sdl/apple/HaylenSceneDelegate.h"

#if !TARGET_OS_OSX
#include "platform/NativeWindow.hpp"
#import "platform/apple/AppleNotifications.hpp"
#import "platform/apple/AppleOrientation.hpp"
#import "platform/apple/ApplePlugins.hpp"
#import "platform/apple/AppleScreens.hpp"
#import "platform/apple/HaylenAccessibilityView.h"
#import "platform/apple/HaylenOverlayLayer.h"
#import "platform/apple/HaylenScreenSceneDelegate.h"

using haylen::platform::AppleNotifications;
#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
using haylen::platform::AppleOrientation;
#endif
using haylen::platform::ApplePlugins;
using haylen::platform::AppleScreens;
using haylen::platform::NativeWindow;

@implementation HaylenSceneDelegate {
    // Whether the scene of this delegate holds the window of the app. UIKit creates a delegate for every scene besides the one of the application.
    BOOL holdsApp;

    // The connection of the scene, which the plugins hear once the window of the app is in it.
    UIScene* connectedScene;
    UISceneSession* connectedSession;
    UISceneConnectionOptions* connectionOptions;
}

// Plugins load before launching ends, so their SDKs set up in time, and the notification center of an app that links UserNotifications gets its delegate as early, so the notification that launched the app reaches the plugins.
- (BOOL)application:(UIApplication*)application willFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id>*)launchOptions {
    ApplePlugins::load();
    AppleNotifications::observe(self);
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application willFinishLaunchingWithOptions:launchOptions];
    }
    return YES;
}

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id>*)launchOptions {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application didFinishLaunchingWithOptions:launchOptions];
    }
    return YES;
}

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
// UIKit reads which questions the application delegate answers once, as the app launches, so the delegate answers this one from the start with the orientations of the Info.plist, and with those of the lock once the app locks the orientation.
- (UIInterfaceOrientationMask)application:(UIApplication*)application supportedInterfaceOrientationsForWindow:(UIWindow*)window {
    return AppleOrientation::getSupported();
}
#endif

// The windows of screens connect with a delegate of their own, so they never become the window of the app.
- (UISceneConfiguration*)application:(UIApplication*)application configurationForConnectingSceneSession:(UISceneSession*)connectingSceneSession options:(UISceneConnectionOptions*)options {
#if !TARGET_OS_TV
    for (NSUserActivity* activity in options.userActivities) {
        if ([activity.activityType isEqualToString:AppleScreens::kWindowActivity]) {
            UISceneConfiguration* configuration = [[UISceneConfiguration alloc] initWithName:@"HaylenScreen" sessionRole:connectingSceneSession.role];
            configuration.delegateClass = HaylenScreenSceneDelegate.class;
            return configuration;
        }
    }
#endif
    UISceneConfiguration* configuration = [[UISceneConfiguration alloc] initWithName:@"Haylen" sessionRole:connectingSceneSession.role];
    configuration.delegateClass = HaylenSceneDelegate.class;
    return configuration;
}

// SDL runs the app in the first scene that connects, which opens the window of the app once SDL starts it, and the plugins hear the connection once the window is in the scene. The app draws in one window, so a scene that connects while the window of the app has a scene, such as a second window that the person opens on an iPad, goes away at once. The window outlives a scene of the app that disconnected, such as the Mac Catalyst window that the person closed while the process goes on, so the next scene of the app takes the window over with the app as it was.
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    UIWindow* window = (__bridge UIWindow*)NativeWindow::getHandle();
    if (window.windowScene != nil) {
        if ([session.role isEqualToString:UIWindowSceneSessionRoleApplication]) {
            [UIApplication.sharedApplication requestSceneSessionDestruction:session options:nil errorHandler:nil];
        }
        return;
    }
    holdsApp = YES;
    connectedScene = scene;
    connectedSession = session;
    connectionOptions = options;
    if (window == nil) {
        [super scene:scene willConnectToSession:session options:options];
        return;
    }
    window.windowScene = static_cast<UIWindowScene*>(scene);
    [window makeKeyAndVisible];
    [HaylenAccessibilityView.shared attachToView:window.rootViewController.view];
    [HaylenOverlayLayer.shared attachToView:window.rootViewController.view];
    [self connectPlugins];
}

// UIKit hands the links, the user activities and the shortcut item that open the app only to the connection options, so the plugins then receive them as they would while the app runs.
- (void)connectPlugins {
    UIScene* scene = connectedScene;
    UISceneConnectionOptions* options = connectionOptions;
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(@selector(scene:willConnectToSession:options:))) {
        [plugin scene:scene willConnectToSession:connectedSession options:options];
    }
    connectedScene = nil;
    connectedSession = nil;
    connectionOptions = nil;

    if (options.URLContexts.count > 0) {
        [self scene:scene openURLContexts:options.URLContexts];
    }
    for (NSUserActivity* activity in options.userActivities) {
        [self scene:scene continueUserActivity:activity];
    }
#if !TARGET_OS_TV
    if (options.shortcutItem != nil) {
        [self windowScene:static_cast<UIWindowScene*>(scene) performActionForShortcutItem:options.shortcutItem completionHandler:^(BOOL){}];
    }
#endif
}

- (void)sceneDidDisconnect:(UIScene*)scene {
    if (!holdsApp) {
        return;
    }
    [HaylenOverlayLayer.shared detach];
    ApplePlugins::closeCovers();
}

// SDL turns the links that open the app into dropped files, which the engine does not read, so only the plugins receive them.
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
    if (!holdsApp) {
        return;
    }
    [super sceneDidBecomeActive:scene];
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin sceneDidBecomeActive:scene];
    }
}

- (void)sceneWillResignActive:(UIScene*)scene {
    if (!holdsApp) {
        return;
    }
    [super sceneWillResignActive:scene];
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin sceneWillResignActive:scene];
    }
}

- (void)sceneWillEnterForeground:(UIScene*)scene {
    if (!holdsApp) {
        return;
    }
    [super sceneWillEnterForeground:scene];
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin sceneWillEnterForeground:scene];
    }
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
    if (!holdsApp) {
        return;
    }
    [super sceneDidEnterBackground:scene];
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

// UIKit warns about a delegate that receives the fetches of remote notifications in an app without the background mode for them, so the delegate takes those fetches only when the `Info.plist` of the app declares `remote-notification` in `UIBackgroundModes`.
- (BOOL)respondsToSelector:(SEL)selector {
    if (selector == @selector(application:didReceiveRemoteNotification:fetchCompletionHandler:)) {
        return HaylenSceneDelegate.fetchesRemoteNotifications;
    }
    return [super respondsToSelector:selector];
}

+ (BOOL)fetchesRemoteNotifications {
    static const BOOL declared = [[NSBundle.mainBundle objectForInfoDictionaryKey:@"UIBackgroundModes"] containsObject:@"remote-notification"];
    return declared;
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

- (void)userNotificationCenter:(id)center willPresentNotification:(id)notification withCompletionHandler:(void (^)(NSUInteger))completionHandler {
    AppleNotifications::present(center, notification, completionHandler);
}

#if !TARGET_OS_TV
- (void)userNotificationCenter:(id)center didReceiveNotificationResponse:(id)response withCompletionHandler:(void (^)(void))completionHandler {
    AppleNotifications::receiveResponse(center, response, completionHandler);
}
#endif

@end
#endif
