#import "platform/apple/HaylenAppDelegate.h"

#if TARGET_OS_OSX
#import "platform/apple/AppleNotifications.hpp"
#import "platform/apple/ApplePlugins.hpp"
#import "platform/apple/AppleTheme.hpp"
#import "platform/apple/HaylenOverlayLayer.h"
#include "sokol_app.h"

using haylen::platform::AppleNotifications;
using haylen::platform::ApplePlugins;
using haylen::platform::AppleTheme;

@implementation HaylenAppDelegate

// The main menu comes first, as an app that loads it from a nib has it. Plugins load before launching ends, so their SDKs set up in time, and the notification center gets its delegate as early, so the notification that launched the app reaches the plugins. Apps without plugins, such as the desktop player, and apps that do not link UserNotifications leave the notification center alone. The appearance of the app follows the one of the system, whose every change the delegate observes.
- (void)applicationWillFinishLaunching:(NSNotification*)notification {
    [self installMainMenu];
    [NSApp addObserver:self forKeyPath:@"effectiveAppearance" options:NSKeyValueObservingOptionInitial context:nil];
    ApplePlugins::load();
    AppleNotifications::observe(self);
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

// The standard menus of a Mac app: the app menu with About, Hide, Hide Others, Show All and Quit, and the Window menu with Minimize, Zoom and Enter Full Screen, which AppKit enables only while the window allows each. An app without a Dock icon has no menu bar, so it quits through its own UI.
- (void)installMainMenu {
    NSString* name = NSRunningApplication.currentApplication.localizedName;
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:name];
    [appMenu addItemWithTitle:[@"About " stringByAppendingString:name] action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItem:NSMenuItem.separatorItem];
    [appMenu addItemWithTitle:[@"Hide " stringByAppendingString:name] action:@selector(hide:) keyEquivalent:@"h"];
    [appMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"].keyEquivalentModifierMask = NSEventModifierFlagOption | NSEventModifierFlagCommand;
    [appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
    [appMenu addItem:NSMenuItem.separatorItem];
    [appMenu addItemWithTitle:[@"Quit " stringByAppendingString:name] action:@selector(terminate:) keyEquivalent:@"q"];

    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [windowMenu addItem:NSMenuItem.separatorItem];
    [windowMenu addItemWithTitle:@"Enter Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"].keyEquivalentModifierMask = NSEventModifierFlagControl | NSEventModifierFlagCommand;

    NSMenu* mainMenu = [NSMenu new];
    for (NSMenu* menu in @[ appMenu, windowMenu ]) {
        [mainMenu addItemWithTitle:menu.title action:nil keyEquivalent:@""].submenu = menu;
    }
    NSApp.mainMenu = mainMenu;
    NSApp.windowsMenu = windowMenu;

    // The app draws in one window, which never gathers others into tabs.
    NSWindow.allowsAutomaticWindowTabbing = NO;
}

// Quit in the menu, Command+Q, the Dock and the system close the window as its close button does, so the app hears `appQuitRequested` and then shuts down like an app that quits itself, whether its frames run or not.
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    NSWindow* window = (__bridge NSWindow*)sapp_macos_get_window();
    if ([window.delegate windowShouldClose:window]) {
        [window close];
    }
    return NSTerminateNow;
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

- (void)userNotificationCenter:(id)center willPresentNotification:(id)notification withCompletionHandler:(void (^)(NSUInteger))completionHandler {
    AppleNotifications::present(center, notification, completionHandler);
}

- (void)userNotificationCenter:(id)center didReceiveNotificationResponse:(id)response withCompletionHandler:(void (^)(void))completionHandler {
    AppleNotifications::receiveResponse(center, response, completionHandler);
}

@end
#endif
