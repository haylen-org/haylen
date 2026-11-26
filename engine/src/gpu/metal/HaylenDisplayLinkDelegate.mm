#import "gpu/metal/HaylenDisplayLinkDelegate.h"

#include <utility>

@implementation HaylenDisplayLinkDelegate {
    std::function<void(id<CAMetalDrawable>)> receive;
}

- (instancetype)initWithUpdate:(std::function<void(id<CAMetalDrawable>)>)update {
    if ((self = [super init])) {
        receive = std::move(update);
    }

    return self;
}

- (void)metalDisplayLink:(CAMetalDisplayLink*)link needsUpdate:(CAMetalDisplayLinkUpdate*)update {
    receive(update.drawable);
}

@end
