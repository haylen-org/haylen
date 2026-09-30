#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

// A hidden view in the window of the app that reports the theme whenever the traits of the window change, such as when the person switches between light and dark.
@interface HaylenThemeView : UIView
@end
#endif
