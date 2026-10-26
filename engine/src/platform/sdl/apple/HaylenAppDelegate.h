#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>

// The application delegate of the runtime on macOS. It gives the app the standard main menu, quits through the engine with the exit status of the app, loads the native plugins while the app launches, places the overlay of the plugins and the elements of screen readers over the window, follows the appearance of the app for its theme, becomes the delegate of the notification center when the app links UserNotifications, and hands the launch, the links that open the app and the events of remote notifications to the plugins.
@interface HaylenAppDelegate : NSObject <NSApplicationDelegate>
@end
#endif
