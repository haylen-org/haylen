#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#import <UserNotifications/UserNotifications.h>

// The application delegate of sokol_app on macOS, which it declares inside its implementation, so the runtime declares it again to derive from it.
@interface _sapp_macos_app_delegate : NSObject <NSApplicationDelegate>
@end

// The application delegate that sokol_app creates for the runtime on macOS. It lets sokol_app handle the events it handles, loads the native plugins while the app launches, places the overlay of the plugins over the window, owns the delegate of the notification center, and hands the launch, the links that open the app and the events of remote notifications to the plugins.
@interface HaylenAppDelegate : _sapp_macos_app_delegate <UNUserNotificationCenterDelegate>
@end
#endif
