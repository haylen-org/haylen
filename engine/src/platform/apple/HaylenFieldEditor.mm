#import "platform/apple/HaylenFieldEditor.h"

#if TARGET_OS_OSX
#include <algorithm>
#include <cstdint>

#include "platform/NativeWindow.hpp"
#import "platform/apple/AppleTextInput.hpp"

using haylen::platform::AppleTextInput;
using haylen::platform::TextInput;

@implementation HaylenFieldEditor {
    std::uint64_t field;
    std::uint64_t revision;
    BOOL multiline;
    BOOL applying;
}

// The view is invisible, so it never corrects, completes, predicts or rewrites what the user types, since the system shows those changes in the view itself or in panels over it that the user would answer without seeing the text they change.
- (instancetype)init {
    self = [super initWithFrame:NSMakeRect(0.0, 0.0, 1.0, 1.0)];
    self.alphaValue = 0.0;
    self.richText = NO;
    self.importsGraphics = NO;
    self.allowsUndo = YES;
    self.automaticQuoteSubstitutionEnabled = NO;
    self.automaticDashSubstitutionEnabled = NO;
    self.automaticSpellingCorrectionEnabled = NO;
    self.continuousSpellCheckingEnabled = NO;
    self.grammarCheckingEnabled = NO;
    self.automaticTextReplacementEnabled = NO;
    self.automaticTextCompletionEnabled = NO;
    self.automaticLinkDetectionEnabled = NO;
    self.automaticDataDetectionEnabled = NO;
    self.smartInsertDeleteEnabled = NO;
    if (@available(macOS 14.0, *)) {
        self.inlinePredictionType = NSTextInputTraitTypeNo;
    }
    if (@available(macOS 15.0, *)) {
        self.mathExpressionCompletionType = NSTextInputTraitTypeNo;
        self.writingToolsBehavior = NSWritingToolsBehaviorNone;
    }
    self.delegate = self;
    return self;
}

- (NSView*)hitTest:(NSPoint)point {
    return nil;
}

// The engine picks the cursor of the pointer itself, the text cursor over the fields of the UI, so the invisible editor never sets one, neither through its cursor rects nor through the moves of the mouse, which reach it as the first responder and go on to the view of the app.
- (void)resetCursorRects {
}

- (void)cursorUpdate:(NSEvent*)event {
}

- (void)mouseMoved:(NSEvent*)event {
    [self.nextResponder mouseMoved:event];
}

- (void)edit:(const TextInput::Field&)value {
    NSWindow* window = (__bridge NSWindow*)haylen::platform::NativeWindow::getHandle();
    NSView* host = window.contentView;
    if (self.superview != host) {
        [host addSubview:self];
    }

    // The view of the app counts from the bottom left, while the engine counts from the top left.
    const CGRect bounds = AppleTextInput::toPoints(value.bounds);
    self.frame = NSMakeRect(bounds.origin.x, host.bounds.size.height - bounds.origin.y - bounds.size.height, std::max<CGFloat>(bounds.size.width, 1.0), std::max<CGFloat>(bounds.size.height, 1.0));
    multiline = value.options.keyboard == TextInput::Keyboard::Multiline;

    // Passwords type with Roman input sources only, like secure text fields.
    const BOOL secure = value.options.keyboard == TextInput::Keyboard::Password;
    self.inputContext.allowedInputSourceLocales = secure ? @[ NSAllRomanInputSourcesLocaleIdentifier ] : nil;

    if (value.id != field || value.revision != revision) {
        field = value.id;
        revision = value.revision;
        applying = YES;
        NSString* text = AppleTextInput::toNSString(value.text);
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
    NSWindow* window = (__bridge NSWindow*)haylen::platform::NativeWindow::getHandle();
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
