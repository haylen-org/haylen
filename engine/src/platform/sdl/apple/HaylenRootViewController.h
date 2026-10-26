#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

// The view controller that SDL makes the root of the window, which SDL declares inside its implementation, so the runtime declares it again to derive from it.
@interface SDL_uikitviewcontroller : UIViewController
@end

// The view controller of the window of the app, which answers UIKit with the status bar that the app asks for. The runtime turns the view controller that SDL created into this class once the window opened, which adds no state to it.
@interface HaylenRootViewController : SDL_uikitviewcontroller
@end
#endif
