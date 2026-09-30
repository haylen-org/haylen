#import "platform/apple/HaylenAppDelegate.h"

#if TARGET_OS_OSX
#import "platform/apple/ApplePlugins.hpp"
#import "platform/apple/AppleTheme.hpp"
#import "platform/apple/HaylenOverlayLayer.h"
#include "sokol_app.h"

using haylen::platform::ApplePlugins;
using haylen::platform::AppleTheme;

@implementation HaylenAppDelegate

// Plugins load before launching ends, so their SDKs set up in time, and the notification center gets its delegate as early, so the notification that launched the app reaches the plugins. Apps without plugins, such as the desktop player, leave the notification center alone. The appearance of the app follows the one of the system, whose every change the delegate observes.
- (void)applicationWillFinishLaunching:(NSNotification*)notification {
    [NSApp addObserver:self forKeyPath:@"effectiveAppearance" options:NSKeyValueObservingOptionInitial context:nil];
    ApplePlugins::load();
    if (!ApplePlugins::getIds().empty()) {
        UNUserNotificationCenter.currentNotificationCenter.delegate = self;
    }
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin applicationWillFinishLaunching:notification];
    }
}

- (void)observeValueForKeyPath:(NSString*)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id>*)change context:(void*)context {
    AppleTheme::report(NSApp.effectiveAppearance);
}

// The library `sokol_app` creates the window here, which the overlay lies over from now on.
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    [super applicationDidFinishLaunching:notification];
    [HaylenOverlayLayer.shared attachToView:((__bridge NSWindow*)sapp_macos_get_window()).contentView];
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin applicationDidFinishLaunching:notification];
    }
}

- (void)application:(NSApplication*)application openURLs:(NSArray<NSURL*>*)urls {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application openURLs:urls];
    }
}

- (void)application:(NSApplication*)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData*)deviceToken {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application didRegisterForRemoteNotificationsWithDeviceToken:deviceToken];
    }
}

- (void)application:(NSApplication*)application didFailToRegisterForRemoteNotificationsWithError:(NSError*)error {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application didFailToRegisterForRemoteNotificationsWithError:error];
    }
}

- (void)application:(NSApplication*)application didReceiveRemoteNotification:(NSDictionary<NSString*, id>*)userInfo {
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin application:application didReceiveRemoteNotification:userInfo];
    }
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center willPresentNotification:(UNNotification*)notification withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler {
    ApplePlugins::presentNotification(center, notification, completionHandler);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center didReceiveNotificationResponse:(UNNotificationResponse*)response withCompletionHandler:(void (^)(void))completionHandler {
    ApplePlugins::receiveNotificationResponse(center, response, completionHandler);
}

@end
#endif
