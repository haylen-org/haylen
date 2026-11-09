#import "platform/apple/HaylenPermission+Runtime.h"

#include <atomic>
#include <memory>

#include "haylen/core/Log.hpp"

@implementation HaylenPermission {
    void (^readState)(HaylenPermissionAnswer answer);
    void (^showPrompt)(HaylenPermissionAnswer answer);
}

- (instancetype)initWithName:(NSString*)name capability:(NSString*)capability requirements:(NSArray<HaylenRequirement*>*)requirements state:(void (^)(HaylenPermissionAnswer answer))state request:(void (^)(HaylenPermissionAnswer answer))request {
    self = [super init];
    _name = [name copy];
    _capability = [capability copy];
    _requirements = [requirements copy];
    readState = [state copy];
    showPrompt = [request copy];
    return self;
}

- (void)answerAsking:(BOOL)asking requirements:(HaylenRequirements*)requirements completion:(void (^)(NSDictionary<NSString*, id>* answer))completion {
    NSError* error = nil;
    if (![requirements require:self.requirements error:&error]) {
        completion(@{@"state" : @"unsupported", @"message" : error.userInfo[@"message"], @"missing" : error.userInfo[@"data"][@"missing"]});
        return;
    }

    HaylenPermissionAnswer answer = [self once:[self finish:completion]];
    HaylenPermissionAnswer afterState = asking ? [self once:[self promptWhenAsked:answer]] : answer;
    void (^state)(HaylenPermissionAnswer) = readState;
    [HaylenPermission runOnMainThread:^{ state(afterState); }];
}

// A state other than `prompt` answers the request without showing the prompt.
- (HaylenPermissionAnswer)promptWhenAsked:(HaylenPermissionAnswer)answer {
    void (^request)(HaylenPermissionAnswer) = showPrompt;
    return ^(HaylenPermissionState state, NSString* _Nullable message) {
      if (state != HaylenPermissionStatePrompt) {
          answer(state, message);
          return;
      }
      [HaylenPermission runOnMainThread:^{ request(answer); }];
    };
}

// A block of the plugin that answers twice is a mistake of the plugin, which the log names, while the first answer stands.
- (HaylenPermissionAnswer)once:(HaylenPermissionAnswer)answer {
    std::shared_ptr<std::atomic<bool>> answered = std::make_shared<std::atomic<bool>>(false);
    NSString* name = self.name;
    return ^(HaylenPermissionState state, NSString* _Nullable message) {
      if (answered->exchange(true)) {
          haylen::core::Log::error("The permission \"{}\" answered more than once, while its blocks answer once.", name.UTF8String);
          return;
      }
      answer(state, message);
    };
}

// A state outside `HaylenPermissionState` is a mistake of the plugin, which answers `unsupported` with the reason.
- (HaylenPermissionAnswer)finish:(void (^)(NSDictionary<NSString*, id>* answer))completion {
    NSString* name = self.name;
    return ^(HaylenPermissionState state, NSString* _Nullable message) {
      NSString* stateName = [HaylenPermission nameOfState:state];
      if (stateName == nil) {
          completion(@{@"state" : @"unsupported", @"message" : [NSString stringWithFormat:@"The permission \"%@\" answered the state %ld, which is none of \"HaylenPermissionState\".", name, static_cast<long>(state)]});
          return;
      }
      if (state == HaylenPermissionStateUnsupported) {
          completion(@{@"state" : stateName, @"message" : message != nil ? message : @""});
          return;
      }
      completion(@{@"state" : stateName});
    };
}

+ (void)runOnMainThread:(dispatch_block_t)work {
    if (NSThread.isMainThread) {
        work();
        return;
    }
    dispatch_async(dispatch_get_main_queue(), work);
}

+ (NSString*)nameOfState:(HaylenPermissionState)state {
    switch (state) {
    case HaylenPermissionStateGranted:
        return @"granted";
    case HaylenPermissionStatePrompt:
        return @"prompt";
    case HaylenPermissionStateDenied:
        return @"denied";
    case HaylenPermissionStateUnknown:
        return @"unknown";
    case HaylenPermissionStateUnsupported:
        return @"unsupported";
    }
    return nil;
}

@end
