#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

// Target of the hover gesture that follows the pointer over the view of a Mac Catalyst app.
@interface HaylenPointer : NSObject

- (void)hover:(UIHoverGestureRecognizer*)recognizer;

@end
#endif
