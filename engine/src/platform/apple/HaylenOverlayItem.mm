#import "platform/apple/HaylenOverlayItem+Runtime.h"

#import "platform/apple/HaylenOverlayLayer.h"

@implementation HaylenOverlayItem {
    BOOL shown;
}

#if TARGET_OS_OSX
- (instancetype)initWithView:(NSView*)content placement:(HaylenPlacement*)where key:(NSString*)name {
#else
- (instancetype)initWithView:(UIView*)content placement:(HaylenPlacement*)where key:(NSString*)name {
#endif
    self = [super init];
    self.view = content;
    self.placement = where;
    self.key = name;
    self.constraints = @[];
    shown = YES;
    return self;
}

- (BOOL)isVisible {
    return shown;
}

- (void)setVisible:(BOOL)value {
    shown = value;
    [HaylenOverlayLayer.shared placeItem:self];
}

// The overlay lays out the views it just placed first, so the bounds hold right after the view was added or moved.
- (CGRect)bounds {
#if TARGET_OS_OSX
    [HaylenOverlayLayer.shared layoutSubtreeIfNeeded];
#else
    [HaylenOverlayLayer.shared layoutIfNeeded];
#endif
    return self.view.frame;
}

- (void)updatePlacement:(HaylenPlacement*)value {
    self.placement = value;
    [HaylenOverlayLayer.shared placeItem:self];
}

- (void)remove {
    [HaylenOverlayLayer.shared removeItem:self];
}

@end
