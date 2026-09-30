#pragma once

#import "haylen/platform/apple/HaylenScreen.h"

#include <cstdint>

#include "haylen/platform/ScreenRequest.hpp"

NS_ASSUME_NONNULL_BEGIN

// What the runtime reaches in a screen beyond its public API: its id, whether it ended, the app giving it up and the UI of the screen going away.
@interface HaylenScreen ()

@property(nonatomic, readonly) std::uint64_t identifier;
@property(nonatomic, readonly, getter=isEnded) BOOL ended;

- (instancetype)initWithRequest:(const haylen::platform::ScreenRequest&)request;

// The app gave the screen up.
- (void)cancel;

// The UI of the screen went away, however it went, which ends a screen that has not ended yet with the code `cancelled`.
- (void)dismissed;

#if !TARGET_OS_OSX && !TARGET_OS_TV
// Returns the window that shows the controller of the window of the screen in the scene that connected for it.
- (UIWindow*)windowForScene:(UIWindowScene*)scene;
#endif

@end

NS_ASSUME_NONNULL_END
