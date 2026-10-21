#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

// The application and scene delegate of `sokol_app`, which it declares inside its implementation, so the runtime declares it again to derive from it.
@interface _sapp_scene_delegate : NSObject <UIApplicationDelegate, UIWindowSceneDelegate>
@end

// The application and scene delegate that `sokol_app` creates for the runtime on iOS, tvOS and Mac Catalyst. It lets `sokol_app` handle the events it handles, loads the native plugins while the app launches, places the overlay of the plugins over the window, follows the theme of the window, becomes the delegate of the notification center when the app links UserNotifications, and hands every event of the app, its scene and its notifications to the plugins. Only the scene that holds the window of the app reaches `sokol_app`, while the windows of screens connect with a delegate of their own and other scenes of the app go away.
@interface HaylenSceneDelegate : _sapp_scene_delegate
@end
#endif
