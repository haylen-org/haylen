#pragma once

#import "haylen/platform/apple/HaylenPermission.h"

NS_ASSUME_NONNULL_BEGIN

// What the runtime reaches in a permission of a plugin beyond its public API.
@interface HaylenPermission ()

// Answers the JSON object `{state, message, missing}` that `Permission::Answer::fromJson` reads, once and on any thread. The requirements that the project lacks answer `unsupported` first, and asking shows the prompt only while the state is `prompt`.
- (void)answerAsking:(BOOL)asking requirements:(HaylenRequirements*)requirements completion:(void (^)(NSDictionary<NSString*, id>* answer))completion;

// The name of the state in `system.permission`, or `nil` for a value outside the enumeration.
+ (nullable NSString*)nameOfState:(HaylenPermissionState)state;

@end

NS_ASSUME_NONNULL_END
