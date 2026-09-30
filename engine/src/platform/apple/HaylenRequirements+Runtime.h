#pragma once

#import "haylen/platform/apple/HaylenRequirements.h"

NS_ASSUME_NONNULL_BEGIN

// What the runtime reaches in the requirements of a plugin beyond their public API.
@interface HaylenRequirements ()

// The owner names what needs the requirements at the start of a sentence, such as `The plugin "share-sheet"`.
- (instancetype)initWithOwner:(NSString*)owner;

@end

NS_ASSUME_NONNULL_END
