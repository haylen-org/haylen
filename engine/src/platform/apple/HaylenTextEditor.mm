#import "platform/apple/HaylenTextEditor.h"

#if !TARGET_OS_OSX
#include <algorithm>
#include <array>
#include <cstdint>

#include "haylen/platform/Event.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"
#import "platform/apple/AppleStrings.hpp"
#import "platform/apple/AppleTextInput.hpp"
#import "platform/apple/HaylenTextField.h"
#import "platform/apple/HaylenTextView.h"

using haylen::platform::AppleStrings;
using haylen::platform::AppleTextInput;
using haylen::platform::TextInput;

#if TARGET_OS_TV
@interface HaylenTextEditor () <UITextFieldDelegate>
@end
#else
@interface HaylenTextEditor () <UITextFieldDelegate, UITextViewDelegate>
@end
#endif

@implementation HaylenTextEditor {
    std::array<HaylenTextField*, 2> textFields;
#if !TARGET_OS_TV
    std::array<HaylenTextView*, 2> textViews;
#endif
    UIView<UITextInput>* current;
    std::uint64_t field;
    std::uint64_t revision;
    int maxLength;
    BOOL editing;
    BOOL applying;
    BOOL finishing;
}

- (instancetype)init {
    self = [super init];
    for (HaylenTextField* __strong& view : textFields) {
        view = [self makeTextField];
    }
#if !TARGET_OS_TV
    for (HaylenTextView* __strong& view : textViews) {
        view = [self makeTextView];
    }

    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    [center addObserver:self selector:@selector(keyboardChanged:) name:UIKeyboardWillChangeFrameNotification object:nil];
    [center addObserver:self selector:@selector(keyboardChanged:) name:UIKeyboardWillHideNotification object:nil];
#endif
    return self;
}

// The editors stay hidden rather than transparent, since the system draws the caret, the selection, the bubbles of autocorrection and the inline predictions of a field in views of its own, which only a hidden field keeps from the screen, and the engine draws the field with its composition itself.
- (HaylenTextField*)makeTextField {
    HaylenTextField* view = [[HaylenTextField alloc] initWithFrame:CGRectMake(0.0, 0.0, 1.0, 1.0)];
    view.editor = self;
    view.delegate = self;
    view.hidden = YES;
    [view addTarget:self action:@selector(textChanged:) forControlEvents:UIControlEventEditingChanged];
    return view;
}

#if !TARGET_OS_TV
- (HaylenTextView*)makeTextView {
    HaylenTextView* view = [[HaylenTextView alloc] initWithFrame:CGRectMake(0.0, 0.0, 1.0, 1.0) textContainer:nil];
    view.editor = self;
    view.delegate = self;
    view.hidden = YES;
    return view;
}
#endif

// The view of the kind of the field: the one that edits it, or for a field that takes over from the one that edits, the view of its kind that does not edit.
- (UIView<UITextInput>*)viewFor:(const TextInput::Field&)value {
#if TARGET_OS_TV
    UIView<UITextInput>* first = textFields[0];
    UIView<UITextInput>* second = textFields[1];
#else
    const bool multiline = value.options.keyboard == TextInput::Keyboard::Multiline;
    UIView<UITextInput>* first = multiline ? textViews[0] : textFields[0];
    UIView<UITextInput>* second = multiline ? textViews[1] : textFields[1];
#endif
    if (current != first && current != second) {
        return first;
    }

    if (!editing || value.id == field) {
        return current;
    }

    return current == first ? second : first;
}

+ (UIKeyboardType)keyboardType:(TextInput::Keyboard)keyboard {
    switch (keyboard) {
    case TextInput::Keyboard::Number:
        return UIKeyboardTypeNumberPad;
    case TextInput::Keyboard::Decimal:
        return UIKeyboardTypeDecimalPad;
    case TextInput::Keyboard::Phone:
        return UIKeyboardTypePhonePad;
    case TextInput::Keyboard::Email:
        return UIKeyboardTypeEmailAddress;
    case TextInput::Keyboard::Url:
        return UIKeyboardTypeURL;
    case TextInput::Keyboard::Search:
        return UIKeyboardTypeWebSearch;
    default:
        return UIKeyboardTypeDefault;
    }
}

+ (UIReturnKeyType)returnKeyType:(TextInput::ReturnKey)key {
    switch (key) {
    case TextInput::ReturnKey::Done:
        return UIReturnKeyDone;
    case TextInput::ReturnKey::Go:
        return UIReturnKeyGo;
    case TextInput::ReturnKey::Next:
        return UIReturnKeyNext;
    case TextInput::ReturnKey::Search:
        return UIReturnKeySearch;
    case TextInput::ReturnKey::Send:
        return UIReturnKeySend;
    case TextInput::ReturnKey::Default:
        return UIReturnKeyDefault;
    }

    return UIReturnKeyDefault;
}

+ (UITextAutocapitalizationType)capitalizationType:(TextInput::Capitalization)capitalization {
    switch (capitalization) {
    case TextInput::Capitalization::Sentences:
        return UITextAutocapitalizationTypeSentences;
    case TextInput::Capitalization::Words:
        return UITextAutocapitalizationTypeWords;
    case TextInput::Capitalization::Characters:
        return UITextAutocapitalizationTypeAllCharacters;
    case TextInput::Capitalization::None:
        return UITextAutocapitalizationTypeNone;
    }

    return UITextAutocapitalizationTypeNone;
}

// Applies the keyboard options to a view and returns whether any changed, which a view that already shows its keyboard must reload. Autocorrection offers its words in the bar of the keyboard, while inline predictions, math completion, smart insertion and the writing tools would change text inside the hidden view that the user never sees, so they stay off.
+ (BOOL)configure:(UIView<UITextInput>*)view options:(const TextInput::Options&)options {
    const UIKeyboardType keyboard = [self keyboardType:options.keyboard];
    const UIReturnKeyType returnKey = [self returnKeyType:options.returnKey];
    const BOOL secure = options.keyboard == TextInput::Keyboard::Password;
    const UITextAutocorrectionType correction = options.autocorrect ? UITextAutocorrectionTypeYes : UITextAutocorrectionTypeNo;
    const UITextAutocapitalizationType capitalization = [self capitalizationType:options.capitalization];
    const BOOL changed = view.keyboardType != keyboard || view.returnKeyType != returnKey || view.isSecureTextEntry != secure || view.autocorrectionType != correction || view.autocapitalizationType != capitalization;
    view.keyboardType = keyboard;
    view.returnKeyType = returnKey;
    view.secureTextEntry = secure;
    view.autocorrectionType = correction;
    view.spellCheckingType = options.autocorrect ? UITextSpellCheckingTypeYes : UITextSpellCheckingTypeNo;
    view.autocapitalizationType = capitalization;
    view.smartQuotesType = UITextSmartQuotesTypeNo;
    view.smartDashesType = UITextSmartDashesTypeNo;
    view.smartInsertDeleteType = UITextSmartInsertDeleteTypeNo;
    if (@available(iOS 17.0, tvOS 17.0, *)) {
        view.inlinePredictionType = UITextInlinePredictionTypeNo;
    }

    if (@available(iOS 18.0, tvOS 18.0, *)) {
        view.mathExpressionCompletionType = UITextMathExpressionCompletionTypeNo;
    }
#if !TARGET_OS_TV
    if (@available(iOS 18.0, visionOS 2.4, *)) {
        view.writingToolsBehavior = UIWritingToolsBehaviorNone;
    }
#endif
    return changed;
}

+ (NSString*)textOf:(UIView<UITextInput>*)view {
    NSString* text = [view textInRange:[view textRangeFromPosition:view.beginningOfDocument toPosition:view.endOfDocument]];
    return text != nil ? text : @"";
}

+ (NSRange)rangeOf:(UITextRange*)range in:(UIView<UITextInput>*)view {
    const NSInteger start = [view offsetFromPosition:view.beginningOfDocument toPosition:range.start];
    const NSInteger end = [view offsetFromPosition:view.beginningOfDocument toPosition:range.end];
    return NSMakeRange(static_cast<NSUInteger>(start), static_cast<NSUInteger>(std::max(start, end) - start));
}

- (void)edit:(const TextInput::Field&)value {
    UIView* host = ((__bridge UIWindow*)haylen::platform::NativeWindow::getHandle()).rootViewController.view;
    UIView<UITextInput>* view = [self viewFor:value];
    const BOOL handingOver = view != current && current.isFirstResponder;
    current = view;

    if (view.superview != host) {
        [host addSubview:view];
    }

    const CGRect frame = AppleTextInput::toPoints(value.bounds);
    view.frame = CGRectMake(frame.origin.x, frame.origin.y, std::max<CGFloat>(frame.size.width, 1.0), std::max<CGFloat>(frame.size.height, 1.0));
    maxLength = value.options.maxLength;
    const BOOL reconfigured = [HaylenTextEditor configure:view options:value.options];

    if (!editing || value.id != field || value.revision != revision) {
        field = value.id;
        revision = value.revision;
        applying = YES;
        NSString* text = AppleStrings::toNSString(value.text);
#if TARGET_OS_TV
        static_cast<UITextField*>(view).text = text;
#else
        if ([view isKindOfClass:UITextView.class]) {
            static_cast<UITextView*>(view).text = text;
        } else {
            static_cast<UITextField*>(view).text = text;
        }
#endif
        UITextPosition* start = [view positionFromPosition:view.beginningOfDocument offset:static_cast<NSInteger>(AppleTextInput::toUtf16(text, value.selectionStart))];
        UITextPosition* end = [view positionFromPosition:view.beginningOfDocument offset:static_cast<NSInteger>(AppleTextInput::toUtf16(text, value.selectionEnd))];
        if (start != nil && end != nil) {
            view.selectedTextRange = [view textRangeFromPosition:start toPosition:end];
        }

        applying = NO;
    }

    editing = YES;

    // A view that takes over from the other one becomes the first responder directly, which ends the editing of the other without a dismissal and without closing the keyboard, as the focus moves between native fields.
    if (!view.isFirstResponder) {
        finishing = handingOver;
        [view becomeFirstResponder];
        finishing = NO;
    } else if (reconfigured) {
        [view reloadInputViews];
    }
}

- (void)finish {
    editing = NO;
    finishing = YES;
    [current resignFirstResponder];
    finishing = NO;
}

- (void)act:(TextInput::Action)action {
    if (editing) {
        AppleTextInput::act(field, action);
    }
}

- (void)report {
    if (!editing || applying || current == nil) {
        return;
    }

    NSString* text = [HaylenTextEditor textOf:current];
    UITextRange* selected = current.selectedTextRange;
    const NSRange selection = selected != nil ? [HaylenTextEditor rangeOf:selected in:current] : NSMakeRange(text.length, 0);
    UITextRange* marked = current.markedTextRange;
    AppleTextInput::report(field, revision, text, selection, marked != nil ? [HaylenTextEditor rangeOf:marked in:current] : NSMakeRange(NSNotFound, 0));
}

// The limit holds while the user types, and marked text of an input method waits until it is committed.
- (BOOL)allowsReplacing:(NSRange)range with:(NSString*)replacement in:(NSString*)text {
    if (maxLength <= 0 || replacement.length == 0 || current.markedTextRange != nil) {
        return YES;
    }

    NSString* next = [text stringByReplacingCharactersInRange:range withString:replacement];
    return AppleTextInput::toCodePoints(next, next.length) <= maxLength;
}

- (void)textChanged:(UITextField*)sender {
    [self report];
}

- (void)textFieldDidChangeSelection:(UITextField*)sender {
    [self report];
}

- (BOOL)textField:(UITextField*)sender shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString*)replacement {
    return [self allowsReplacing:range with:replacement in:sender.text != nil ? sender.text : @""];
}

- (BOOL)textFieldShouldReturn:(UITextField*)sender {
    [self act:TextInput::Action::Submit];
    return NO;
}

- (void)textFieldDidEndEditing:(UITextField*)sender {
    if (!finishing) {
        [self act:TextInput::Action::Dismissed];
    }
}

#if !TARGET_OS_TV
- (void)textViewDidChange:(UITextView*)sender {
    [self report];
}

- (void)textViewDidChangeSelection:(UITextView*)sender {
    [self report];
}

- (BOOL)textView:(UITextView*)sender shouldChangeTextInRange:(NSRange)range replacementText:(NSString*)replacement {
    return [self allowsReplacing:range with:replacement in:sender.text != nil ? sender.text : @""];
}

- (void)textViewDidEndEditing:(UITextView*)sender {
    if (!finishing) {
        [self act:TextInput::Action::Dismissed];
    }
}

// The keyboard frame reaches the engine in framebuffer pixels of the view of the app, and a hidden keyboard covers nothing.
- (void)keyboardChanged:(NSNotification*)notification {
    const CGRect frame = [self coveredFrame:notification];
    const haylen::math::Rect area = CGRectIsNull(frame) || CGRectIsEmpty(frame) ? haylen::math::Rect{} : AppleTextInput::toPixels(frame);
    haylen::platform::Player::postEvent({.type = haylen::platform::Event::Type::KeyboardChanged, .keyboardFrame = area});
}

// The part of the view of the app that the keyboard covers, in points. The keyboard of visionOS floats apart from the window of the app and covers nothing of it.
- (CGRect)coveredFrame:(NSNotification*)notification {
#if TARGET_OS_VISION
    return CGRectZero;
#else
    UIView* host = ((__bridge UIWindow*)haylen::platform::NativeWindow::getHandle()).rootViewController.view;
    if ([notification.name isEqualToString:UIKeyboardWillHideNotification] || host.window == nil) {
        return CGRectZero;
    }

    const CGRect screen = [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    return CGRectIntersection([host convertRect:screen fromCoordinateSpace:host.window.windowScene.screen.coordinateSpace], host.bounds);
#endif
}
#endif

@end
#endif
