#pragma once

#import "haylen/platform/apple/HaylenPlugin.h"

NS_ASSUME_NONNULL_BEGIN

// The view over the view of the app that holds the views of every plugin overlay. It lets touches and clicks through to the app everywhere but on those views, places them with Auto Layout against its safe area or its edges, and reserves the edges of the views whose placement asks for it, in framebuffer pixels, whenever its layout changes.
#if TARGET_OS_OSX
@interface HaylenOverlayLayer : NSView
#else
@interface HaylenOverlayLayer : UIView
#endif

@property(class, nonatomic, readonly) HaylenOverlayLayer* shared;

// Lies over the view of the app from now on, which exists once the window of the app does.
#if TARGET_OS_OSX
- (void)attachToView:(NSView*)host;
#else
- (void)attachToView:(UIView*)host;
#endif
- (void)detach;

- (void)addItem:(HaylenOverlayItem*)item;
- (void)placeItem:(HaylenOverlayItem*)item;
- (void)removeItem:(HaylenOverlayItem*)item;

@end

NS_ASSUME_NONNULL_END
