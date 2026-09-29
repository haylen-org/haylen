#pragma once

#import "haylen/platform/apple/HaylenPlugin.h"

NS_ASSUME_NONNULL_BEGIN

// What the overlay layer keeps for each view it places: the view, its placement, the key of its reservation and the constraints that place it.
@interface HaylenOverlayItem ()

#if TARGET_OS_OSX
@property(nonatomic) NSView* view;
#else
@property(nonatomic) UIView* view;
#endif
@property(nonatomic, copy) HaylenPlacement* placement;
@property(nonatomic, copy) NSString* key;
@property(nonatomic, copy) NSArray<NSLayoutConstraint*>* constraints;

#if TARGET_OS_OSX
- (instancetype)initWithView:(NSView*)view placement:(HaylenPlacement*)placement key:(NSString*)key;
#else
- (instancetype)initWithView:(UIView*)view placement:(HaylenPlacement*)placement key:(NSString*)key;
#endif

@end

NS_ASSUME_NONNULL_END
