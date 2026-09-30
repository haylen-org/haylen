#pragma once

#import "haylen/platform/apple/HaylenRequirements.h"

NS_ASSUME_NONNULL_BEGIN

// What the requirements of a plugin reach in a requirement beyond its public API.
@interface HaylenRequirement ()

// What the requirement is, as the log and the failure name it, such as `the usage description "NSCameraUsageDescription"`.
@property(nonatomic, readonly, copy) NSString* summary;

// The entry of `data.missing` that tells the app what the project lacks.
@property(nonatomic, readonly) NSDictionary<NSString*, NSString*>* entry;

- (BOOL)isMetBy:(HaylenRequirements*)requirements;

@end

NS_ASSUME_NONNULL_END
