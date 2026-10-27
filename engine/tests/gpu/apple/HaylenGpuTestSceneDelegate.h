#pragma once

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// The delegate of the scene of the GPU test app, which shows an empty window and starts the tests once the scene is active.
@interface HaylenGpuTestSceneDelegate : UIResponder <UIWindowSceneDelegate>

@property(nonatomic, strong, nullable) UIWindow* window;

@end

NS_ASSUME_NONNULL_END
