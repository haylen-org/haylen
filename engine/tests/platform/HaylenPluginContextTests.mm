#include <gtest/gtest.h>

#include <string>

#import <Foundation/Foundation.h>

#import "platform/apple/HaylenPluginContext+Runtime.h"

namespace haylen::platform {

// The completion runs on the main thread whatever thread asks, even for a name that the plugin never registered, since Swift checks that a block of the main actor runs there.
TEST(HaylenPluginContextTest, FailsAnUnregisteredPermissionOnTheMainThread) {
    HaylenPluginContext* context = [[HaylenPluginContext alloc] initWithIdentifier:@"camera-scanner" config:@{}];
    dispatch_semaphore_t answered = dispatch_semaphore_create(0);
    __block BOOL onMainThread = NO;
    __block NSError* failure = nil;

    // clang-format off
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        [context requirePermission:@"camera" completion:^(NSError* error) {
            onMainThread = NSThread.isMainThread;
            failure = error;
            dispatch_semaphore_signal(answered);
        }];
    });
    // clang-format on

    // The main queue runs its blocks only while the main run loop turns.
    NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:10];
    bool done = false;
    while (!done && deadline.timeIntervalSinceNow > 0) {
        [NSRunLoop.mainRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
        done = dispatch_semaphore_wait(answered, DISPATCH_TIME_NOW) == 0;
    }

    ASSERT_TRUE(done);
    EXPECT_TRUE(onMainThread);
    ASSERT_NE(failure, nil);
    NSString* code = failure.userInfo[@"code"];
    NSString* message = failure.userInfo[@"message"];
    EXPECT_EQ(std::string(code.UTF8String), "unsupported");
    EXPECT_EQ(std::string(message.UTF8String), "The plugin \"camera-scanner\" requires the permission \"camera\", which it did not register with \"registerPermission\".");
}

} // namespace haylen::platform
