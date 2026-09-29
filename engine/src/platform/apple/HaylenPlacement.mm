#import "haylen/platform/apple/HaylenPlugin.h"

@implementation HaylenPlacement

- (instancetype)init {
    self = [super init];
    self.anchor = HaylenPlacementAnchorBottom;
    self.insideSafeArea = YES;
    return self;
}

- (instancetype)initWithAnchor:(HaylenPlacementAnchor)anchor {
    self = [self init];
    self.anchor = anchor;
    return self;
}

- (id)copyWithZone:(NSZone*)zone {
    HaylenPlacement* copy = [[HaylenPlacement allocWithZone:zone] initWithAnchor:self.anchor];
    copy.margin = self.margin;
    copy.insideSafeArea = self.insideSafeArea;
    copy.reserve = self.reserve;
    copy.width = self.width;
    copy.height = self.height;
    return copy;
}

@end
