#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

// The application and scene delegate of sokol_app, which it declares inside its implementation, so the runtime declares it again to derive from it.
@interface _sapp_scene_delegate : NSObject <UIApplicationDelegate, UIWindowSceneDelegate>
@end

// The application and scene delegate that sokol_app creates for the runtime on iOS, tvOS and Mac Catalyst. It lets sokol_app handle the events it handles, loads the native plugins while the app launches, places the overlay of the plugins over the window, owns the delegate of the notification center, and hands every event of the app, its scene and its notifications to the plugins.
@interface HaylenSceneDelegate : _sapp_scene_delegate <UNUserNotificationCenterDelegate>
@end
#endif
