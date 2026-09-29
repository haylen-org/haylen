#import "platform/apple/HaylenFieldEditor.h"

#if TARGET_OS_OSX
#include <algorithm>
#include <cstdint>

#import "platform/apple/AppleTextInput.hpp"
#include "sokol_app.h"

using haylen::platform::AppleTextInput;
using haylen::platform::TextInput;

@implementation HaylenFieldEditor {
    std::uint64_t field;
    std::uint64_t revision;
    BOOL multiline;
    BOOL applying;
}

- (instancetype)init {
    self = [super initWithFrame:NSMakeRect(0.0, 0.0, 1.0, 1.0)];
    self.alphaValue = 0.0;
    self.richText = NO;
    self.importsGraphics = NO;
    self.allowsUndo = YES;
    self.automaticQuoteSubstitutionEnabled = NO;
    self.automaticDashSubstitutionEnabled = NO;
    self.delegate = self;
    return self;
}

- (NSView*)hitTest:(NSPoint)point {
    return nil;
}

- (void)edit:(const TextInput::Field&)value {
    NSWindow* window = (__bridge NSWindow*)sapp_macos_get_window();
    NSView* host = window.contentView;
    if (self.superview != host) {
        [host addSubview:self];
    }

    // The view of the app counts from the bottom left, while the engine counts from the top left.
    const CGRect bounds = AppleTextInput::toPoints(value.bounds);
    self.frame = NSMakeRect(bounds.origin.x, host.bounds.size.height - bounds.origin.y - bounds.size.height, std::max<CGFloat>(bounds.size.width, 1.0), std::max<CGFloat>(bounds.size.height, 1.0));
    multiline = value.options.keyboard == TextInput::Keyboard::Multiline;
    self.automaticSpellingCorrectionEnabled = value.options.autocorrect;
    self.continuousSpellCheckingEnabled = value.options.autocorrect;
    self.automaticTextReplacementEnabled = value.options.autocorrect;

    // Passwords type with Roman input sources only, like secure text fields.
    const BOOL secure = value.options.keyboard == TextInput::Keyboard::Password;
    self.inputContext.allowedInputSourceLocales = secure ? @[ NSAllRomanInputSourcesLocaleIdentifier ] : nil;

    if (value.id != field || value.revision != revision) {
        field = value.id;
        revision = value.revision;
        applying = YES;
        NSString* text = [NSString stringWithUTF8String:value.text.c_str()];
        self.string = text;
        const NSUInteger start = AppleTextInput::toUtf16(text, value.selectionStart);
        self.selectedRange = NSMakeRange(start, AppleTextInput::toUtf16(text, value.selectionEnd) - start);
        applying = NO;
    }
    if (window.firstResponder != self) {
        [window makeFirstResponder:self];
    }
}

- (void)finish {
    NSWindow* window = (__bridge NSWindow*)sapp_macos_get_window();
    if (window.firstResponder == self) {
        [window makeFirstResponder:window.contentView];
    }
    field = TextInput::kKeyboardField;
    revision = 0;
}

- (void)report {
    if (applying) {
        return;
    }
    AppleTextInput::report(field, revision, self.string, self.selectedRange, self.hasMarkedText ? self.markedRange : NSMakeRange(NSNotFound, 0));
}

- (void)textDidChange:(NSNotification*)notification {
    [self report];
}

- (void)textViewDidChangeSelection:(NSNotification*)notification {
    [self report];
}

// Return submits a single line and breaks a text area, tab moves on, escape cancels, and the text never takes a tab.
- (BOOL)textView:(NSTextView*)textView doCommandBySelector:(SEL)command {
    if (command == @selector(insertNewline:) && !multiline) {
        AppleTextInput::act(field, TextInput::Action::Submit);
        return YES;
    }
    if (command == @selector(insertTab:)) {
        AppleTextInput::act(field, TextInput::Action::Next);
        return YES;
    }
    if (command == @selector(insertBacktab:)) {
        return YES;
    }
    if (command == @selector(cancelOperation:)) {
        AppleTextInput::act(field, TextInput::Action::Cancel);
        return YES;
    }
    return NO;
}

@end
#endif
