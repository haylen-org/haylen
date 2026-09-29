#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

@class HaylenTextEditor;

// The hidden single line field that edits the focused field on iOS, tvOS and Mac Catalyst. It turns tab and escape of a hardware keyboard into actions of its editor.
@interface HaylenTextField : UITextField

@property(nonatomic, weak) HaylenTextEditor* editor;

@end
#endif
