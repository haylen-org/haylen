#import "platform/apple/HaylenOverlay+Runtime.h"

#import "platform/apple/HaylenOverlayItem+Runtime.h"
#import "platform/apple/HaylenOverlayLayer.h"

@implementation HaylenOverlay {
    NSString* plugin;
}

- (instancetype)initWithIdentifier:(NSString*)identifier {
    self = [super init];
    plugin = [identifier copy];
    return self;
}

#if TARGET_OS_OSX
- (HaylenOverlayItem*)addView:(NSView*)view placement:(HaylenPlacement*)placement {
#else
- (HaylenOverlayItem*)addView:(UIView*)view placement:(HaylenPlacement*)placement {
#endif
    HaylenOverlayItem* item = [[HaylenOverlayItem alloc] initWithView:view placement:placement key:[HaylenOverlay nextKeyOf:plugin]];
    [HaylenOverlayLayer.shared addItem:item];
    return item;
}

// Names the reservation of a new view after its plugin, uniquely among the views of every overlay.
+ (NSString*)nextKeyOf:(NSString*)identifier {
    static NSUInteger serial = 0;
    return [NSString stringWithFormat:@"%@#%lu", identifier, static_cast<unsigned long>(++serial)];
}

@end
