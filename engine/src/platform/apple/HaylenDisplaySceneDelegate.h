#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// The delegate of the scene that UIKit connects for an external display of an iPhone or an iPad, which tells the app that the display connected or disconnected, and ends the output that the scene shows when it goes. The window of an output belongs to `AppleOutputs`.
@interface HaylenDisplaySceneDelegate : UIResponder <UIWindowSceneDelegate>

@end

NS_ASSUME_NONNULL_END
#endif
