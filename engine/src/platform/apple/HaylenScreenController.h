#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>

@class HaylenScreen;

NS_ASSUME_NONNULL_BEGIN

// The container that the runtime presents for the controller of a screen, which it embeds as its child. It sees every way it goes away and tells the screen: the end of the screen, which dismisses it, the swipe of the person, which its presentation delegate hears, and a dismissal by the controller, by SwiftUI or by an SDK, which its disappearance shows. It takes the status bar, the orientations, the size and the focus of the controller, and blocks the swipe while the controller asks for modal presentation.
@interface HaylenScreenController : UIViewController <UIAdaptivePresentationControllerDelegate>

- (instancetype)initWithScreen:(HaylenScreen*)screen content:(UIViewController*)content style:(UIModalPresentationStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(nullable NSString*)nibNameOrNil bundle:(nullable NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (nullable instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
#endif
