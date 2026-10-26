#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

// The application and scene delegate of SDL, which SDL declares inside its implementation, so the runtime declares it again to derive from it.
@interface SDLUIKitSceneDelegate : NSObject <UIApplicationDelegate, UIWindowSceneDelegate>
@end

// The application and scene delegate that UIKit creates for the runtime on iOS, iPadOS, Mac Catalyst, tvOS and visionOS. It lets SDL run the app in the scene that holds its window, loads the native plugins while the app launches, becomes the delegate of the notification center when the app links UserNotifications, and hands every event of the app, its scene and its notifications to the plugins. Only the scene that holds the window of the app reaches SDL, while the windows of screens connect with a delegate of their own and other scenes of the app go away.
@interface HaylenSceneDelegate : SDLUIKitSceneDelegate

// Hands the plugins the connection of the scene, with the links, user activities and shortcut item that opened the app, once the window of the app is in the scene.
- (void)connectPlugins;

@end
#endif
