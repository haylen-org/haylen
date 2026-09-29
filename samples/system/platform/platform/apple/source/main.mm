#import "haylen/platform/apple/HaylenBridge.h"
#import "haylen/platform/apple/HaylenMain.h"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

// Answers sample.echo and sample.ticker, and sends sample.activity when the app becomes active or resigns.
@interface SamplePlugin : NSObject
+ (void)registerHandlers;
@end

@implementation SamplePlugin

+ (NSString*)systemName {
#if TARGET_OS_OSX
    return [@"macOS " stringByAppendingString:NSProcessInfo.processInfo.operatingSystemVersionString];
#else
    return [NSString stringWithFormat:@"%@ %@", UIDevice.currentDevice.systemName, UIDevice.currentDevice.systemVersion];
#endif
}

+ (void)sendActivity:(NSString*)state {
    [HaylenBridge emit:@"sample.activity" payload:@{@"state" : state, @"source" : @"Objective-C"}];
}

+ (void)registerHandlers {
    HaylenHandler echo = ^(id params, HaylenReply reply) {
      id text = [params isKindOfClass:NSDictionary.class] ? params[@"text"] : nil;
      if (![text isKindOfClass:NSString.class] || [text length] == 0) {
          reply(NO, @"sample.echo needs a text.");
          return;
      }
      NSUInteger characters = [text lengthOfBytesUsingEncoding:NSUTF32StringEncoding] / 4;
      reply(YES, @{@"echo" : text, @"characters" : @(characters), @"language" : @"Objective-C", @"system" : [SamplePlugin systemName]});
    };
    [HaylenBridge registerHandler:@"sample.echo" handler:echo];

    HaylenHandler ticker = ^(id params, HaylenReply reply) {
      NSDictionary* options = [params isKindOfClass:NSDictionary.class] ? params : @{};
      NSInteger count = MAX(1, [options[@"count"] integerValue] ?: 5);
      NSInteger interval = MAX(1, [options[@"interval"] integerValue] ?: 500);
      for (NSInteger tick = 1; tick <= count; ++tick) {
          NSDictionary* payload = @{@"count" : @(tick), @"total" : @(count), @"source" : @"Objective-C"};
          dispatch_after(dispatch_time(DISPATCH_TIME_NOW, tick * interval * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ [HaylenBridge emit:@"sample.tick" payload:payload]; });
      }
      reply(YES, @{@"started" : @YES, @"count" : @(count), @"interval" : @(interval)});
    };
    [HaylenBridge registerHandler:@"sample.ticker" handler:ticker];

#if TARGET_OS_OSX
    NSNotificationName active = NSApplicationDidBecomeActiveNotification;
    NSNotificationName resigned = NSApplicationWillResignActiveNotification;
#else
    NSNotificationName active = UIApplicationDidBecomeActiveNotification;
    NSNotificationName resigned = UIApplicationWillResignActiveNotification;
#endif
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    [center addObserverForName:active object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { [SamplePlugin sendActivity:@"active"]; }];
    [center addObserverForName:resigned object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { [SamplePlugin sendActivity:@"resigned"]; }];
}

@end

// Registers the bridge methods of the sample, then starts the app with the package that the project copies to Resources/app.
int main(int argc, char* argv[]) {
    [SamplePlugin registerHandlers];
    return haylen_main(argc, argv);
}
