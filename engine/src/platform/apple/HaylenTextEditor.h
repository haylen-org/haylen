#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

#include "haylen/platform/TextInput.hpp"

// Edits the focused field on iOS, tvOS and Mac Catalyst with a hidden text field, or a hidden text view for text areas, placed over the field in the view of the app. It keeps their text, selection and marked text in step with the engine and reports the frame of the software keyboard. On tvOS the field presents the fullscreen keyboard.
@interface HaylenTextEditor : NSObject

- (void)edit:(const haylen::platform::TextInput::Field&)field;
- (void)finish;
- (void)act:(haylen::platform::TextInput::Action)action;

@end
#endif
