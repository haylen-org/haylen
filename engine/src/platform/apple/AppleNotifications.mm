#import "platform/apple/AppleNotifications.hpp"

#import "platform/apple/ApplePlugins.hpp"

// The methods of `UNUserNotificationCenter` and of `HaylenNotificationPlugin` that the runtime calls, with `id` in place of the types of UserNotifications.
@protocol HaylenNotificationCenter <NSObject>

+ (id)currentNotificationCenter;
- (void)setDelegate:(id)delegate;

@optional

- (void)userNotificationCenter:(id)center willPresentNotification:(id)notification withCompletionHandler:(void (^)(NSUInteger options))completionHandler;
- (void)userNotificationCenter:(id)center didReceiveNotificationResponse:(id)response withCompletionHandler:(void (^)(void))completionHandler;

@end

namespace haylen::platform {

void AppleNotifications::observe(id delegate) {
    Class center = NSClassFromString(@"UNUserNotificationCenter");
    if (center == nil || ApplePlugins::getIds().empty()) {
        return;
    }
    [[center currentNotificationCenter] setDelegate:delegate];
}

void AppleNotifications::present(id center, id notification, void (^completionHandler)(NSUInteger)) {
    // clang-format off
    ApplePlugins::join(@selector(userNotificationCenter:willPresentNotification:withCompletionHandler:), ^(NSUInteger options) { completionHandler(options); }, ^(id<HaylenPlugin> plugin, ApplePlugins::Answer answer) {
        id receiver = plugin;
        [receiver userNotificationCenter:center willPresentNotification:notification withCompletionHandler:^(NSUInteger options) { answer(options); }];
    });
    // clang-format on
}

#if !TARGET_OS_TV
void AppleNotifications::receiveResponse(id center, id response, void (^completionHandler)(void)) {
    // clang-format off
    ApplePlugins::join(@selector(userNotificationCenter:didReceiveNotificationResponse:withCompletionHandler:), ^(NSUInteger) { completionHandler(); }, ^(id<HaylenPlugin> plugin, ApplePlugins::Answer answer) {
        id receiver = plugin;
        [receiver userNotificationCenter:center didReceiveNotificationResponse:response withCompletionHandler:^{ answer(0); }];
    });
    // clang-format on
}
#endif

} // namespace haylen::platform
