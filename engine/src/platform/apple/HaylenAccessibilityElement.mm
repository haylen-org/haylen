#import "platform/apple/HaylenAccessibilityElement.h"

#include "platform/AccessibilityRelay.hpp"

using haylen::platform::AccessibilityElement;
using haylen::platform::AccessibilityRelay;

@implementation HaylenAccessibilityElement {
    std::uint8_t actions;
}

#if TARGET_OS_OSX
- (instancetype)initWithIdentifier:(std::uint64_t)identifier parent:(id)parent {
    self = [super init];
    _identifier = identifier;
    self.accessibilityParent = parent;
    return self;
}
#else
- (instancetype)initWithIdentifier:(std::uint64_t)identifier container:(id)container {
    self = [super initWithAccessibilityContainer:container];
    _identifier = identifier;
    return self;
}
#endif

- (void)perform:(AccessibilityElement::Action)action {
    AccessibilityRelay::perform({.element = _identifier, .action = action});
}

#if TARGET_OS_OSX
// The roles of AppKit for the roles of the engine, where switches and tabs are check boxes and radio buttons of their own subrole.
- (void)update:(const AccessibilityElement&)element frame:(CGRect)frame {
    using Role = AccessibilityElement::Role;
    actions = element.actions;
    NSAccessibilityRole role = NSAccessibilityGroupRole;
    NSAccessibilitySubrole subrole = nil;
    switch (element.role) {
    case Role::Group:
    case Role::PlayArea:
        break;
    case Role::Text:
    case Role::Heading:
    case Role::Alert:
        role = NSAccessibilityStaticTextRole;
        break;
    case Role::Image:
        role = NSAccessibilityImageRole;
        break;
    case Role::Button:
        role = NSAccessibilityButtonRole;
        break;
    case Role::CheckBox:
        role = NSAccessibilityCheckBoxRole;
        break;
    case Role::Switch:
        role = NSAccessibilityCheckBoxRole;
        subrole = NSAccessibilitySwitchSubrole;
        break;
    case Role::RadioGroup:
        role = NSAccessibilityRadioGroupRole;
        break;
    case Role::RadioButton:
        role = NSAccessibilityRadioButtonRole;
        break;
    case Role::TabList:
        role = NSAccessibilityTabGroupRole;
        break;
    case Role::Tab:
        role = NSAccessibilityRadioButtonRole;
        subrole = NSAccessibilityTabButtonSubrole;
        break;
    case Role::Slider:
        role = NSAccessibilitySliderRole;
        break;
    case Role::TextField:
        role = NSAccessibilityTextFieldRole;
        break;
    case Role::ComboBox:
        role = NSAccessibilityPopUpButtonRole;
        break;
    case Role::ProgressBar:
        role = NSAccessibilityProgressIndicatorRole;
        break;
    case Role::List:
        role = NSAccessibilityListRole;
        break;
    case Role::Dialog:
        subrole = NSAccessibilityDialogSubrole;
        break;
    }
    self.accessibilityRole = role;
    self.accessibilitySubrole = subrole;
    self.accessibilityLabel = [NSString stringWithUTF8String:element.label.c_str()];
    self.accessibilityHelp = element.hint.empty() ? nil : [NSString stringWithUTF8String:element.hint.c_str()];
    const bool checkable = element.role == Role::CheckBox || element.role == Role::Switch || element.role == Role::RadioButton || element.role == Role::Tab;
    if (checkable) {
        self.accessibilityValue = @(element.has(AccessibilityElement::Checked) || element.has(AccessibilityElement::Selected) ? 1 : 0);
    } else {
        self.accessibilityValue = element.value.empty() ? nil : [NSString stringWithUTF8String:element.value.c_str()];
    }
    self.accessibilityEnabled = !element.has(AccessibilityElement::Disabled);
    self.accessibilitySelected = element.has(AccessibilityElement::Selected);
    self.accessibilityExpanded = element.has(AccessibilityElement::Expanded);
    self.accessibilityFrameInParentSpace = frame;
}

- (BOOL)accessibilityPerformPress {
    if ((actions & AccessibilityElement::Activate) == 0) {
        return NO;
    }
    [self perform:AccessibilityElement::Activate];
    return YES;
}

- (BOOL)accessibilityPerformIncrement {
    if ((actions & AccessibilityElement::Increment) == 0) {
        return NO;
    }
    [self perform:AccessibilityElement::Increment];
    return YES;
}

- (BOOL)accessibilityPerformDecrement {
    if ((actions & AccessibilityElement::Decrement) == 0) {
        return NO;
    }
    [self perform:AccessibilityElement::Decrement];
    return YES;
}

// VoiceOver focuses an element as it moves to it, which moves the focus of the UI there.
- (void)setAccessibilityFocused:(BOOL)focused {
    [super setAccessibilityFocused:focused];
    if (focused && (actions & AccessibilityElement::Focus) != 0) {
        [self perform:AccessibilityElement::Focus];
    }
}
#else
// The traits of UIKit for the roles and states of the engine. Switches and check boxes are toggle buttons where the system has the trait, and selected buttons before, and a text field, for which UIKit has no trait, says its label and its text.
- (void)update:(const AccessibilityElement&)element frame:(CGRect)frame {
    using Role = AccessibilityElement::Role;
    actions = element.actions;
    UIAccessibilityTraits traits = UIAccessibilityTraitNone;
    switch (element.role) {
    case Role::Group:
    case Role::RadioGroup:
    case Role::TabList:
    case Role::List:
    case Role::Dialog:
    case Role::TextField:
        break;
    case Role::Text:
    case Role::Alert:
        traits |= UIAccessibilityTraitStaticText;
        break;
    case Role::Heading:
        traits |= UIAccessibilityTraitHeader;
        break;
    case Role::Image:
        traits |= UIAccessibilityTraitImage;
        break;
    case Role::Button:
    case Role::RadioButton:
    case Role::ComboBox:
        traits |= UIAccessibilityTraitButton;
        break;
    case Role::Tab:
        traits |= UIAccessibilityTraitTabBar;
        break;
    case Role::CheckBox:
    case Role::Switch:
        traits |= UIAccessibilityTraitButton;
        if (@available(iOS 17, tvOS 17, *)) {
            traits |= UIAccessibilityTraitToggleButton;
        }
        break;
    case Role::Slider:
        traits |= UIAccessibilityTraitAdjustable;
        break;
    case Role::ProgressBar:
        traits |= UIAccessibilityTraitUpdatesFrequently;
        break;
    case Role::PlayArea:
        traits |= UIAccessibilityTraitAllowsDirectInteraction;
        break;
    }
    if (element.has(AccessibilityElement::Checked) || element.has(AccessibilityElement::Selected)) {
        traits |= UIAccessibilityTraitSelected;
    }
    if (element.has(AccessibilityElement::Disabled)) {
        traits |= UIAccessibilityTraitNotEnabled;
    }
    if ((element.actions & (AccessibilityElement::Increment | AccessibilityElement::Decrement)) != 0) {
        traits |= UIAccessibilityTraitAdjustable;
    }
    self.accessibilityTraits = traits;
    self.accessibilityLabel = [NSString stringWithUTF8String:element.label.c_str()];
    self.accessibilityValue = element.value.empty() ? nil : [NSString stringWithUTF8String:element.value.c_str()];
    self.accessibilityHint = element.hint.empty() ? nil : [NSString stringWithUTF8String:element.hint.c_str()];
    self.accessibilityFrameInContainerSpace = frame;
}

- (BOOL)accessibilityActivate {
    if ((actions & AccessibilityElement::Activate) == 0) {
        return NO;
    }
    [self perform:AccessibilityElement::Activate];
    return YES;
}

- (void)accessibilityIncrement {
    [self perform:AccessibilityElement::Increment];
}

- (void)accessibilityDecrement {
    [self perform:AccessibilityElement::Decrement];
}

// VoiceOver focuses an element as it moves to it, which moves the focus of the UI there.
- (void)accessibilityElementDidBecomeFocused {
    if ((actions & AccessibilityElement::Focus) != 0) {
        [self perform:AccessibilityElement::Focus];
    }
}
#endif

@end
