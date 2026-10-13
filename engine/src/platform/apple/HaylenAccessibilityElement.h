#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include "haylen/platform/AccessibilityElement.hpp"

NS_ASSUME_NONNULL_BEGIN

// An element of the UI of the app as the screen readers of Apple platforms see it, which hands the actions they ask of it to the engine. The view of the elements keeps one object for every element id, so a screen reader keeps its place while the tree changes.
#if TARGET_OS_OSX
@interface HaylenAccessibilityElement : NSAccessibilityElement
#else
@interface HaylenAccessibilityElement : UIAccessibilityElement
#endif

@property(nonatomic, readonly) std::uint64_t identifier;

#if TARGET_OS_OSX
- (instancetype)initWithIdentifier:(std::uint64_t)identifier parent:(id)parent;
#else
- (instancetype)initWithIdentifier:(std::uint64_t)identifier container:(id)container;
#endif

// Takes what the element is, says and can do, with its bounds in points of the view of the elements.
- (void)update:(const haylen::platform::AccessibilityElement&)element frame:(CGRect)frame;

@end

NS_ASSUME_NONNULL_END
