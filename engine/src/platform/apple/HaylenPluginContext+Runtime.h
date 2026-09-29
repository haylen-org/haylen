#pragma once

#import "haylen/platform/apple/HaylenPlugin.h"

NS_ASSUME_NONNULL_BEGIN

// What the runtime reaches in the context of a plugin beyond its public API.
@interface HaylenPluginContext ()

- (instancetype)initWithIdentifier:(NSString*)identifier config:(NSDictionary<NSString*, id>*)config;

// Ends the covers that the plugin left open.
- (void)closeCovers;

@end

NS_ASSUME_NONNULL_END
