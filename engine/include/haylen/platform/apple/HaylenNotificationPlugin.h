#pragma once

#import <UserNotifications/UserNotifications.h>

#import "haylen/platform/apple/HaylenPlugin.h"

NS_ASSUME_NONNULL_BEGIN

// A plugin that posts or receives notifications, which the runtime hands the events of the notification center on the main thread, in load order. The runtime refers to UserNotifications only through the Objective-C runtime, so it finds the notification center while the app launches only when the app links the framework, as a plugin that uses it does. When the app has plugins, it then makes its application delegate the delegate of the center, which the system needs to deliver the response to a notification that launched the app, so plugins implement the methods of the delegate here and never replace it.
NS_SWIFT_UI_ACTOR
@protocol HaylenNotificationPlugin <HaylenPlugin>

@optional

// A notification that arrives while the app is in front shows with the union of the options that the plugins ask for, or not at all when none asks.
- (void)userNotificationCenter:(UNUserNotificationCenter*)center willPresentNotification:(UNNotification*)notification withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler;
- (void)userNotificationCenter:(UNUserNotificationCenter*)center didReceiveNotificationResponse:(UNNotificationResponse*)response withCompletionHandler:(void (^)(void))completionHandler API_UNAVAILABLE(tvos);

@end

NS_ASSUME_NONNULL_END
