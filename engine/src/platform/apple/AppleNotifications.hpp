#pragma once

#include <TargetConditionals.h>

#import <Foundation/Foundation.h>

namespace haylen::platform {

// The notification center of UserNotifications, which the runtime reaches through the Objective-C runtime alone, with `id` in place of the types of the framework, so an app links UserNotifications only when it uses it, as the notification plugins do. The application delegate of each platform becomes the delegate of the center and hands its events to the plugins that implement the methods of `HaylenNotificationPlugin`.
class AppleNotifications final {
  public:
    // Makes the application delegate the delegate of the notification center while the app launches, when the app links UserNotifications and has plugins, so the response to a notification that launched the app reaches the plugins.
    static void observe(id delegate);

    // A notification that arrives while the app is in front shows with the union of the options that the plugins ask for.
    static void present(id center, id notification, void (^completionHandler)(NSUInteger options));
#if !TARGET_OS_TV
    static void receiveResponse(id center, id response, void (^completionHandler)(void));
#endif
};

} // namespace haylen::platform
