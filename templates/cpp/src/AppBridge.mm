#import <Foundation/Foundation.h>

#import "haylen/platform/apple/HaylenBridge.h"

// The native platform bridge handlers of the app on macOS, iOS, iPadOS, Mac Catalyst and tvOS, which register when the runtime loads the class, before the app starts.
@interface AppBridge : NSObject
@end

@implementation AppBridge

// Answers "app.greet" with a greeting for the name that the app sends, and sends "app.greeted" with the count of greetings so far.
+ (void)load {
    __block NSInteger greetings = 0;
    [HaylenBridge registerHandler:@"app.greet"
                          handler:^(id params, HaylenReply reply) {
                            NSString* name = [params isKindOfClass:NSDictionary.class] ? params[@"name"] : @"";
                            greetings += 1;
                            reply(YES, @{@"message" : [NSString stringWithFormat:@"Hello, %@, from Objective-C.", name]});
                            [HaylenBridge emit:@"app.greeted" payload:@{@"count" : @(greetings)}];
                          }];
}

@end
