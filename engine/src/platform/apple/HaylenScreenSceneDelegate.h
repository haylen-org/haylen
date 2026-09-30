#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX && !TARGET_OS_TV
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// The delegate of the scenes that show the windows of screens on Mac Catalyst and iPad, which never hold the window of the app. A scene that connects for a screen that no longer shows, such as one that the system restores when the app launches, goes away at once, and so does the session of a window that closed.
@interface HaylenScreenSceneDelegate : UIResponder <UIWindowSceneDelegate>

@property(nonatomic, strong, nullable) UIWindow* window;

@end

NS_ASSUME_NONNULL_END
#endif
