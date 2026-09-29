#import "platform/apple/HaylenPointer.h"

#if TARGET_OS_MACCATALYST
#include "platform/apple/CatalystInput.hpp"

@implementation HaylenPointer

- (void)hover:(UIHoverGestureRecognizer*)recognizer {
    haylen::platform::CatalystInput::trackPointer(recognizer);
}

@end
#endif
