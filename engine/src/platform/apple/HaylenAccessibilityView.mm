#import "platform/apple/HaylenAccessibilityView.h"

#include "platform/NativeWindow.hpp"
#import "platform/apple/HaylenAccessibilityElement.h"

using haylen::platform::AccessibilityElement;

@implementation HaylenAccessibilityView {
    NSMutableDictionary<NSNumber*, HaylenAccessibilityElement*>* known;
    NSArray<HaylenAccessibilityElement*>* shown;
    std::uint64_t focusedId;
    BOOL asked;
}

+ (HaylenAccessibilityView*)shared {
    static HaylenAccessibilityView* view = [[HaylenAccessibilityView alloc] initWithFrame:CGRectZero];
    return view;
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    known = [NSMutableDictionary dictionary];
    shown = @[];
    self.translatesAutoresizingMaskIntoConstraints = NO;
#if !TARGET_OS_OSX
    self.userInteractionEnabled = NO;
    self.isAccessibilityElement = NO;
#endif
    return self;
}

#if TARGET_OS_OSX
- (void)attachToView:(NSView*)host {
#else
- (void)attachToView:(UIView*)host {
#endif
    [host addSubview:self];
    [NSLayoutConstraint activateConstraints:@[
        [self.leftAnchor constraintEqualToAnchor:host.leftAnchor],
        [self.rightAnchor constraintEqualToAnchor:host.rightAnchor],
        [self.topAnchor constraintEqualToAnchor:host.topAnchor],
        [self.bottomAnchor constraintEqualToAnchor:host.bottomAnchor],
    ]];
}

- (BOOL)wanted {
#if TARGET_OS_OSX
    return asked || NSWorkspace.sharedWorkspace.voiceOverEnabled;
#else
    return asked || UIAccessibilityIsVoiceOverRunning() || UIAccessibilityIsSwitchControlRunning();
#endif
}

// The elements keep their objects by id, and a change of the ids or of the focus tells the screen reader that the layout changed, with the element to move to when the focus moved.
- (void)updateElements:(std::span<const AccessibilityElement>)elements focused:(std::uint64_t)focused {
    const CGFloat scale = haylen::platform::NativeWindow::getScale();
    NSMutableDictionary<NSNumber*, HaylenAccessibilityElement*>* kept = [NSMutableDictionary dictionaryWithCapacity:elements.size()];
    NSMutableArray<HaylenAccessibilityElement*>* ordered = [NSMutableArray arrayWithCapacity:elements.size()];
    BOOL moved = elements.size() != shown.count;
    for (const AccessibilityElement& element : elements) {
        NSNumber* key = @(element.id);
        HaylenAccessibilityElement* object = known[key];
        if (object == nil) {
#if TARGET_OS_OSX
            object = [[HaylenAccessibilityElement alloc] initWithIdentifier:element.id parent:self];
#else
            object = [[HaylenAccessibilityElement alloc] initWithIdentifier:element.id container:self];
#endif
            moved = YES;
        }
        CGRect frame = CGRectMake(element.bounds.x / scale, element.bounds.y / scale, element.bounds.width / scale, element.bounds.height / scale);
#if TARGET_OS_OSX
        // AppKit places the frames of elements from the bottom of their parent.
        frame.origin.y = self.bounds.size.height - CGRectGetMaxY(frame);
#endif
        [object update:element frame:frame];
        kept[key] = object;
        [ordered addObject:object];
    }
    known = kept;
    shown = ordered;

    const BOOL refocused = focused != focusedId;
    focusedId = focused;
    HaylenAccessibilityElement* target = focused != 0 ? known[@(focused)] : nil;
#if TARGET_OS_OSX
    if (moved) {
        NSAccessibilityPostNotification(self, NSAccessibilityLayoutChangedNotification);
    }
    if (refocused && target != nil) {
        NSAccessibilityPostNotification(target, NSAccessibilityFocusedUIElementChangedNotification);
    }
#else
    if (moved || (refocused && target != nil)) {
        UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification, refocused ? target : nil);
    }
#endif
}

- (void)announce:(NSString*)text interrupt:(BOOL)interrupt {
#if TARGET_OS_OSX
    id element = self.window != nil ? static_cast<id>(self.window) : static_cast<id>(NSApp);
    NSAccessibilityPostNotificationWithUserInfo(element, NSAccessibilityAnnouncementRequestedNotification, @{NSAccessibilityAnnouncementKey : text, NSAccessibilityPriorityKey : @(interrupt ? NSAccessibilityPriorityHigh : NSAccessibilityPriorityMedium)});
#else
    NSAttributedString* announcement = [[NSAttributedString alloc] initWithString:text attributes:@{UIAccessibilitySpeechAttributeQueueAnnouncement : @(!interrupt)}];
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification, announcement);
#endif
}

#if TARGET_OS_OSX
// The view lets every click through to the app and stands for the UI as a group whose children are its elements.
- (NSView*)hitTest:(NSPoint)point {
    return nil;
}

- (BOOL)isAccessibilityElement {
    return YES;
}

- (NSAccessibilityRole)accessibilityRole {
    return NSAccessibilityGroupRole;
}

- (NSArray*)accessibilityChildren {
    asked = YES;
    return shown;
}
#else
- (NSArray*)accessibilityElements {
    asked = YES;
    return shown;
}

- (NSInteger)accessibilityElementCount {
    asked = YES;
    return static_cast<NSInteger>(shown.count);
}

- (id)accessibilityElementAtIndex:(NSInteger)index {
    return shown[static_cast<NSUInteger>(index)];
}

- (NSInteger)indexOfAccessibilityElement:(id)element {
    const NSUInteger index = [shown indexOfObject:element];
    return index == NSNotFound ? NSNotFound : static_cast<NSInteger>(index);
}
#endif

@end
