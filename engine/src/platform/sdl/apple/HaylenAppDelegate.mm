#import "platform/sdl/apple/HaylenAppDelegate.h"

#if TARGET_OS_OSX
#include <cstdlib>

#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"
#import "platform/apple/AppleAccessibility.hpp"
#import "platform/apple/AppleNotifications.hpp"
#import "platform/apple/ApplePlugins.hpp"
#import "platform/apple/AppleTheme.hpp"
#import "platform/apple/HaylenAccessibilityView.h"
#import "platform/apple/HaylenOverlayLayer.h"
#include "platform/sdl/Runtime.hpp"

using haylen::platform::AppleAccessibility;
using haylen::platform::AppleNotifications;
using haylen::platform::ApplePlugins;
using haylen::platform::AppleTheme;
using haylen::platform::NativeWindow;
using haylen::platform::Player;
using haylen::platform::sdl::Runtime;

@implementation HaylenAppDelegate

// The main menu comes first, as an app that loads it from a nib has it. Plugins load before launching ends, so their SDKs set up in time, and the notification center gets its delegate as early, so the notification that launched the app reaches the plugins. Apps without plugins, such as the desktop player, and apps that do not link UserNotifications leave the notification center alone. The appearance of the app follows the one of the system, whose every change the delegate observes, and so does the state of VoiceOver, which no notification announces.
- (void)applicationWillFinishLaunching:(NSNotification*)notification {
    [self installMainMenu];
    [NSApp addObserver:self forKeyPath:@"effectiveAppearance" options:NSKeyValueObservingOptionInitial context:nil];
    [NSWorkspace.sharedWorkspace addObserver:self forKeyPath:@"voiceOverEnabled" options:NSKeyValueObservingOptionNew context:nil];
    ApplePlugins::load();
    AppleNotifications::observe(self);
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin applicationWillFinishLaunching:notification];
    }
}

- (void)observeValueForKeyPath:(NSString*)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id>*)change context:(void*)context {
    if (object == NSWorkspace.sharedWorkspace) {
        AppleAccessibility::report();
        return;
    }
    AppleTheme::report(NSApp.effectiveAppearance);
}

// The runtime finishes launching once the window is open, which the elements of screen readers and the overlay lie over from now on, and brings the app to the front, also when a terminal launched it.
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    NSView* content = ((__bridge NSWindow*)NativeWindow::getHandle()).contentView;
    [HaylenAccessibilityView.shared attachToView:content];
    [HaylenOverlayLayer.shared attachToView:content];
    [NSApp activateIgnoringOtherApps:YES];
    for (id<HaylenPlugin> plugin in ApplePlugins::getPlugins(_cmd)) {
        [plugin applicationDidFinishLaunching:notification];
    }
}

// The standard menus of a Mac app: the app menu with About, Hide, Hide Others, Show All and Quit, the Edit menu with Undo, Redo, Cut, Copy, Paste, Delete and Select All, and the Window menu with Minimize, Zoom and Enter Full Screen, which AppKit enables only while the window allows each. The Edit menu gives the Command shortcuts to the native field that edits a text field of the UI, and while no field edits, AppKit disables its items and the keys reach the app. An app without a Dock icon has no menu bar, so it quits through its own UI.
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

    NSMenu* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
    [editMenu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
    [editMenu addItem:NSMenuItem.separatorItem];
    [editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Delete" action:@selector(delete:) keyEquivalent:@""];
    [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];

    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [windowMenu addItem:NSMenuItem.separatorItem];
    [windowMenu addItemWithTitle:@"Enter Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"].keyEquivalentModifierMask = NSEventModifierFlagControl | NSEventModifierFlagCommand;

    NSMenu* mainMenu = [NSMenu new];
    for (NSMenu* menu in @[ appMenu, editMenu, windowMenu ]) {
        [mainMenu addItemWithTitle:menu.title action:nil keyEquivalent:@""].submenu = menu;
    }
    NSApp.mainMenu = mainMenu;
    NSApp.windowsMenu = windowMenu;

    // The app draws in one window, which never gathers others into tabs.
    NSWindow.allowsAutomaticWindowTabbing = NO;
}

// Quit in the menu, Command+Q, the Dock and the system close the window as its close button does, so the app hears `appQuitRequested` and then shuts down like an app that quits itself, whether its frames run or not.
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    if (Runtime* runtime = Runtime::find()) {
        runtime->terminate();
    }
    return NSTerminateNow;
}

// AppKit ends the process with the status 0 once the app terminated, so an app that quit with another status ends it here, after the runtime stopped the app.
- (void)applicationWillTerminate:(NSNotification*)notification {
    if (const int status = Player::getExitStatus(); status != 0) {
        std::exit(status);
    }
}

// The app keeps no state of its own for AppKit to restore, so AppKit may keep the state of its windows with secure coding.
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)application {
    return YES;
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
