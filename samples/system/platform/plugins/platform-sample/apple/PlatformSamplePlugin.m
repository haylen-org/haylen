#import "haylen/platform/apple/HaylenPlugin.h"

#include <TargetConditionals.h>

// Native part of the platform sample on Apple platforms, in Objective-C. It answers platform-sample.echo and platform-sample.ticker, and sends platform-sample.activity when the app becomes active or resigns.
@interface PlatformSamplePlugin : NSObject <HaylenPlugin>
@end

@implementation PlatformSamplePlugin

+ (NSString*)systemName {
#if TARGET_OS_OSX
    return [@"macOS " stringByAppendingString:NSProcessInfo.processInfo.operatingSystemVersionString];
#else
    return [NSString stringWithFormat:@"%@ %@", UIDevice.currentDevice.systemName, UIDevice.currentDevice.systemVersion];
#endif
}

- (void)loadWithContext:(HaylenPluginContext*)context {
    __weak HaylenPluginContext* weakContext = context;
    HaylenHandler echo = ^(id params, HaylenReply reply) {
      id text = [params isKindOfClass:NSDictionary.class] ? params[@"text"] : nil;
      if (![text isKindOfClass:NSString.class] || [text length] == 0) {
          reply(NO, @"The method \"platform-sample.echo\" needs a text.");
          return;
      }
      NSUInteger characters = [text lengthOfBytesUsingEncoding:NSUTF32StringEncoding] / 4;
      reply(YES, @{@"echo" : text, @"characters" : @(characters), @"language" : @"Objective-C", @"system" : [PlatformSamplePlugin systemName]});
    };
    [context registerHandler:@"echo" handler:echo];

    HaylenHandler ticker = ^(id params, HaylenReply reply) {
      NSDictionary* options = [params isKindOfClass:NSDictionary.class] ? params : @{};
      NSInteger count = MAX(1, [options[@"count"] integerValue] ?: 5);
      NSInteger interval = MAX(1, [options[@"interval"] integerValue] ?: 500);
      for (NSInteger tick = 1; tick <= count; ++tick) {
          NSDictionary* payload = @{@"count" : @(tick), @"total" : @(count), @"source" : @"Objective-C"};
          dispatch_after(dispatch_time(DISPATCH_TIME_NOW, tick * interval * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ [weakContext emit:@"tick" payload:payload]; });
      }
      reply(YES, @{@"started" : @YES, @"count" : @(count), @"interval" : @(interval)});
    };
    [context registerHandler:@"ticker" handler:ticker];

#if TARGET_OS_OSX
    NSNotificationName active = NSApplicationDidBecomeActiveNotification;
    NSNotificationName resigned = NSApplicationWillResignActiveNotification;
#else
    NSNotificationName active = UIApplicationDidBecomeActiveNotification;
    NSNotificationName resigned = UIApplicationWillResignActiveNotification;
#endif
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    [center addObserverForName:active object:nil queue:NSOperationQueue.mainQueue usingBlock:^(__unused NSNotification* notification) { [weakContext emit:@"activity" payload:@{@"state" : @"active", @"source" : @"Objective-C"}]; }];
    [center addObserverForName:resigned object:nil queue:NSOperationQueue.mainQueue usingBlock:^(__unused NSNotification* notification) { [weakContext emit:@"activity" payload:@{@"state" : @"resigned", @"source" : @"Objective-C"}]; }];
}

@end
