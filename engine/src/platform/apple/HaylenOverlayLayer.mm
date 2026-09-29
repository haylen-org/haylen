#import "platform/apple/HaylenOverlayLayer.h"

#include "haylen/math/Insets.hpp"
#import "platform/apple/HaylenOverlayItem+Runtime.h"
#include "platform/sokol/SokolHost.hpp"
#include "sokol_app.h"

using haylen::math::Insets;
using haylen::platform::SokolHost;

@implementation HaylenOverlayLayer {
    NSMutableArray<HaylenOverlayItem*>* items;
#if TARGET_OS_OSX
    NSLayoutGuide* edges;
#else
    UILayoutGuide* edges;
#endif
}

+ (HaylenOverlayLayer*)shared {
    static HaylenOverlayLayer* layer = [[HaylenOverlayLayer alloc] initWithFrame:CGRectZero];
    return layer;
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    items = [NSMutableArray array];
    self.translatesAutoresizingMaskIntoConstraints = NO;
#if TARGET_OS_OSX
    edges = [[NSLayoutGuide alloc] init];
#else
    edges = [[UILayoutGuide alloc] init];
#endif
    [self addLayoutGuide:edges];
    [NSLayoutConstraint activateConstraints:@[
        [edges.leftAnchor constraintEqualToAnchor:self.leftAnchor],
        [edges.rightAnchor constraintEqualToAnchor:self.rightAnchor],
        [edges.topAnchor constraintEqualToAnchor:self.topAnchor],
        [edges.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];
    return self;
}

#if TARGET_OS_OSX
- (void)attachToView:(NSView*)host {
#else
- (void)attachToView:(UIView*)host {
#endif
    [host addSubview:self];
    [NSLayoutConstraint activateConstraints:@[
        [self.leftAnchor constraintEqualToAnchor:host.leftAnchor],
        [self.rightAnchor constraintEqualToAnchor:host.rightAnchor],
        [self.topAnchor constraintEqualToAnchor:host.topAnchor],
        [self.bottomAnchor constraintEqualToAnchor:host.bottomAnchor],
    ]];
}

// Views reserve their edges only while they show over the app, and the next attach reserves them again.
- (void)detach {
    [self removeFromSuperview];
    for (HaylenOverlayItem* item in items) {
        SokolHost::getNativeViews().releaseInsets(item.key.UTF8String);
    }
}

- (void)addItem:(HaylenOverlayItem*)item {
    [items addObject:item];
    item.view.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:item.view];
    [self placeItem:item];
}

// A removed item stays removed, whatever its placement and visibility become.
- (void)placeItem:(HaylenOverlayItem*)item {
    if (![items containsObject:item]) {
        return;
    }
    [NSLayoutConstraint deactivateConstraints:item.constraints];
    item.constraints = [self constraintsFor:item];
    [NSLayoutConstraint activateConstraints:item.constraints];
    item.view.hidden = !item.visible;
#if TARGET_OS_OSX
    self.needsLayout = YES;
#else
    [self setNeedsLayout];
#endif
}

- (void)removeItem:(HaylenOverlayItem*)item {
    if (![items containsObject:item]) {
        return;
    }
    [items removeObject:item];
    [item.view removeFromSuperview];
    SokolHost::getNativeViews().releaseInsets(item.key.UTF8String);
}

#if TARGET_OS_OSX
// The layer counts from the top left corner like the engine and the other platforms.
- (BOOL)isFlipped {
    return YES;
}

- (NSView*)hitTest:(NSPoint)point {
    NSView* hit = [super hitTest:point];
    return hit == self ? nil : hit;
}

- (void)layout {
    [super layout];
    [self reserveEdges];
}
#else
- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
    UIView* hit = [super hitTest:point withEvent:event];
    return hit == self ? nil : hit;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    [self reserveEdges];
}

// The remote of the TV and the keyboard keep driving the app, so no view of the layer takes the focus.
- (BOOL)shouldUpdateFocusInContext:(UIFocusUpdateContext*)context {
    UIView* next = context.nextFocusedView;
    return [super shouldUpdateFocusInContext:context] && (next == nil || ![next isDescendantOfView:self]);
}
#endif

// The margin applies to the edges the anchor names, and the view keeps its own size along an axis whose size the placement leaves at zero.
- (NSArray<NSLayoutConstraint*>*)constraintsFor:(HaylenOverlayItem*)item {
    HaylenPlacement* placement = item.placement;
#if TARGET_OS_OSX
    NSLayoutGuide* guide = placement.insideSafeArea ? self.safeAreaLayoutGuide : edges;
    NSView* view = item.view;
#else
    UILayoutGuide* guide = placement.insideSafeArea ? self.safeAreaLayoutGuide : edges;
    UIView* view = item.view;
#endif
    const CGPoint alignment = [HaylenOverlayLayer alignmentOf:placement.anchor];
    const CGFloat margin = placement.margin;
    NSMutableArray<NSLayoutConstraint*>* constraints = [NSMutableArray array];

    if (alignment.x < 0.5) {
        [constraints addObject:[view.leftAnchor constraintEqualToAnchor:guide.leftAnchor constant:margin]];
    } else if (alignment.x > 0.5) {
        [constraints addObject:[view.rightAnchor constraintEqualToAnchor:guide.rightAnchor constant:-margin]];
    } else {
        [constraints addObject:[view.centerXAnchor constraintEqualToAnchor:guide.centerXAnchor]];
    }
    if (alignment.y < 0.5) {
        [constraints addObject:[view.topAnchor constraintEqualToAnchor:guide.topAnchor constant:margin]];
    } else if (alignment.y > 0.5) {
        [constraints addObject:[view.bottomAnchor constraintEqualToAnchor:guide.bottomAnchor constant:-margin]];
    } else {
        [constraints addObject:[view.centerYAnchor constraintEqualToAnchor:guide.centerYAnchor]];
    }

    if (placement.width > 0.0) {
        [constraints addObject:[view.widthAnchor constraintEqualToConstant:placement.width]];
    }
    if (placement.height > 0.0) {
        [constraints addObject:[view.heightAnchor constraintEqualToConstant:placement.height]];
    }
    return constraints;
}

// A visible view reserves the edge its anchor names, from the edge of the app to its far side, like the overlay of the web runtime. A centered view reserves nothing.
- (void)reserveEdges {
    const CGSize size = self.bounds.size;
    const CGFloat scale = sapp_dpi_scale();
    for (HaylenOverlayItem* item in items) {
        HaylenPlacement* placement = item.placement;
        if (!item.visible || !placement.reserve || placement.anchor == HaylenPlacementAnchorCenter) {
            SokolHost::getNativeViews().releaseInsets(item.key.UTF8String);
            continue;
        }

        const CGRect frame = item.view.frame;
        const CGPoint alignment = [HaylenOverlayLayer alignmentOf:placement.anchor];
        Insets insets;
        if (alignment.y < 0.5) {
            insets.top = static_cast<float>(CGRectGetMaxY(frame) * scale);
        } else if (alignment.y > 0.5) {
            insets.bottom = static_cast<float>((size.height - CGRectGetMinY(frame)) * scale);
        } else if (alignment.x < 0.5) {
            insets.left = static_cast<float>(CGRectGetMaxX(frame) * scale);
        } else {
            insets.right = static_cast<float>((size.width - CGRectGetMinX(frame)) * scale);
        }
        SokolHost::getNativeViews().reserveInsets(item.key.UTF8String, insets);
    }
}

// Where an anchor sits along each axis: 0 at the left or top, 0.5 in the middle and 1 at the right or bottom.
+ (CGPoint)alignmentOf:(HaylenPlacementAnchor)anchor {
    switch (anchor) {
    case HaylenPlacementAnchorTop:
        return CGPointMake(0.5, 0.0);
    case HaylenPlacementAnchorBottom:
        return CGPointMake(0.5, 1.0);
    case HaylenPlacementAnchorLeft:
        return CGPointMake(0.0, 0.5);
    case HaylenPlacementAnchorRight:
        return CGPointMake(1.0, 0.5);
    case HaylenPlacementAnchorTopLeft:
        return CGPointMake(0.0, 0.0);
    case HaylenPlacementAnchorTopRight:
        return CGPointMake(1.0, 0.0);
    case HaylenPlacementAnchorBottomLeft:
        return CGPointMake(0.0, 1.0);
    case HaylenPlacementAnchorBottomRight:
        return CGPointMake(1.0, 1.0);
    case HaylenPlacementAnchorCenter:
        return CGPointMake(0.5, 0.5);
    }
    return CGPointMake(0.5, 0.5);
}

@end
