#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX && !TARGET_OS_TV
#import <UIKit/UIKit.h>

@class HaylenTextEditor;

// The hidden text view that edits a focused text area on iOS and Mac Catalyst. It turns tab and escape of a hardware keyboard into actions of its editor.
@interface HaylenTextView : UITextView

@property(nonatomic, weak) HaylenTextEditor* editor;

@end
#endif
