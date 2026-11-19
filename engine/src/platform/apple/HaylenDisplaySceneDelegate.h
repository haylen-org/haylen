#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// The delegate of the scene that UIKit connects for an external display of an iPhone or an iPad, which tells the app that the display connected or disconnected, and ends the output that the scene shows when it goes. The scene shows its black window from the moment it connects, since UIKit replaces the first scene of an external display right after that scene shows a window, so the replacement ends before the app opens an output, which `AppleOutputs` shows in that window.
@interface HaylenDisplaySceneDelegate : UIResponder <UIWindowSceneDelegate>

@property(nonatomic, strong, nullable) UIWindow* window;

@end

NS_ASSUME_NONNULL_END
#endif
