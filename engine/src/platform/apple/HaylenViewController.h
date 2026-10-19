#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

// The view controller of the window of the app, which holds the Metal view of `sokol_app` and answers UIKit with the status bar that the app asks for.
@interface HaylenViewController : UIViewController
@end
#endif
