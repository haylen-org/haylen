#import "platform/apple/AppleAccessibility.hpp"

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#include "platform/SystemState.hpp"
#include "platform/sokol/SokolHost.hpp"

namespace haylen::platform {

void AppleAccessibility::observe() {
#if TARGET_OS_OSX
    [NSWorkspace.sharedWorkspace.notificationCenter addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { report(); }];
#else
    for (NSNotificationName name in @[ UIAccessibilityReduceMotionStatusDidChangeNotification, UIAccessibilityDarkerSystemColorsStatusDidChangeNotification, UIAccessibilityReduceTransparencyStatusDidChangeNotification, UIAccessibilityVoiceOverStatusDidChangeNotification, UIContentSizeCategoryDidChangeNotification ]) {
        [NSNotificationCenter.defaultCenter addObserverForName:name object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) { report(); }];
    }
#endif
    report();
}

void AppleAccessibility::report() {
    SokolHost::getSystemState().setAccessibility(read());
}

// The text scale is the size of body text at the preferred content size of the app over its size at the default category, which macOS has no setting for.
AccessibilitySettings AppleAccessibility::read() {
#if TARGET_OS_OSX
    NSWorkspace* workspace = NSWorkspace.sharedWorkspace;
    return {.textScale = 1.0F, .reducedMotion = workspace.accessibilityDisplayShouldReduceMotion == YES, .highContrast = workspace.accessibilityDisplayShouldIncreaseContrast == YES, .reducedTransparency = workspace.accessibilityDisplayShouldReduceTransparency == YES, .screenReader = workspace.voiceOverEnabled == YES};
#else
    constexpr CGFloat kBodySize = 17.0;
    UITraitCollection* traits = [UITraitCollection traitCollectionWithPreferredContentSizeCategory:UIApplication.sharedApplication.preferredContentSizeCategory];
    const CGFloat scaled = [UIFontMetrics.defaultMetrics scaledValueForValue:kBodySize compatibleWithTraitCollection:traits];
    return {.textScale = static_cast<float>(scaled / kBodySize), .reducedMotion = UIAccessibilityIsReduceMotionEnabled() == YES, .highContrast = UIAccessibilityDarkerSystemColorsEnabled() == YES, .reducedTransparency = UIAccessibilityIsReduceTransparencyEnabled() == YES, .screenReader = UIAccessibilityIsVoiceOverRunning() == YES};
#endif
}

} // namespace haylen::platform
