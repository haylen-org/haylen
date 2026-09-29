#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>

#include "haylen/platform/TextInput.hpp"

// The invisible text view that edits the focused field on macOS while it is the first responder of the window, with marked text, dead keys, the emoji picker and dictation. Clicks pass through it to the view of the app, which places the caret itself.
@interface HaylenFieldEditor : NSTextView <NSTextViewDelegate>

- (void)edit:(const haylen::platform::TextInput::Field&)field;
- (void)finish;

@end
#endif
