#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include <cstdint>
#include <span>

#include "haylen/platform/AccessibilityElement.hpp"

NS_ASSUME_NONNULL_BEGIN

// The view over the view of the app whose accessibility elements are the elements of the UI, which lets every touch and click through to the app. An assistive technology that asks the view for its elements, such as VoiceOver or Accessibility Inspector, makes the engine build the tree from then on.
#if TARGET_OS_OSX
@interface HaylenAccessibilityView : NSView
#else
@interface HaylenAccessibilityView : UIView
#endif

@property(class, nonatomic, readonly) HaylenAccessibilityView* shared;

// Whether an assistive technology asked for the elements since the process started, or VoiceOver runs.
@property(nonatomic, readonly) BOOL wanted;

#if TARGET_OS_OSX
- (void)attachToView:(NSView*)host;
#else
- (void)attachToView:(UIView*)host;
#endif

// Takes the elements of the UI, with bounds in framebuffer pixels, and the id of the element that holds the focus or 0, on the main thread.
- (void)updateElements:(std::span<const haylen::platform::AccessibilityElement>)elements focused:(std::uint64_t)focused;

- (void)announce:(NSString*)text interrupt:(BOOL)interrupt;

@end

NS_ASSUME_NONNULL_END
