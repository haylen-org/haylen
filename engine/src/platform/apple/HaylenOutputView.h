#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

#include <cstdint>

NS_ASSUME_NONNULL_BEGIN

// The view of an output on an external display, whose layer is the Metal layer that the GPU device draws the output into, and which reports every new size of its drawable.
@interface HaylenOutputView : UIView

- (instancetype)initWithOutput:(std::uint32_t)output frame:(CGRect)frame;

@end

NS_ASSUME_NONNULL_END
#endif
