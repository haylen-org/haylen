#pragma once

#import "haylen/platform/apple/HaylenPlugin.h"

NS_ASSUME_NONNULL_BEGIN

// The overlay of one plugin, whose views reserve edges under keys that start with the id of the plugin.
@interface HaylenOverlay ()

- (instancetype)initWithIdentifier:(NSString*)identifier;

@end

NS_ASSUME_NONNULL_END
