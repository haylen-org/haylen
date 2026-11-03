#import "platform/apple/HaylenOutputView.h"

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import "platform/apple/AppleOutputs.hpp"

using haylen::platform::AppleOutputs;

@implementation HaylenOutputView {
    std::uint32_t number;
}

// The layer is the one that the Metal device draws into, which the platform names by its class alone, since only the GPU devices include the headers of a graphics API.
+ (Class)layerClass {
    return NSClassFromString(@"CAMetalLayer");
}

- (instancetype)initWithOutput:(std::uint32_t)output frame:(CGRect)frame {
    self = [super initWithFrame:frame];
    number = output;
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    AppleOutputs::resize(number);
}

@end
#endif
