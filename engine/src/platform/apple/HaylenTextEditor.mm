#import "platform/apple/HaylenTextEditor.h"

#if !TARGET_OS_OSX
#include <algorithm>
#include <cstdint>

#include "haylen/platform/Event.hpp"
#import "platform/apple/AppleTextInput.hpp"
#import "platform/apple/HaylenTextField.h"
#import "platform/apple/HaylenTextView.h"
#include "platform/sokol/SokolRuntime.hpp"
#include "sokol_app.h"

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
    HaylenTextField* textField;
#if !TARGET_OS_TV
    HaylenTextView* textView;
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
    textField = [[HaylenTextField alloc] initWithFrame:CGRectMake(0.0, 0.0, 1.0, 1.0)];
    textField.editor = self;
    textField.delegate = self;
    textField.alpha = 0.0;
    [textField addTarget:self action:@selector(textChanged:) forControlEvents:UIControlEventEditingChanged];
#if !TARGET_OS_TV
    textView = [[HaylenTextView alloc] initWithFrame:CGRectMake(0.0, 0.0, 1.0, 1.0) textContainer:nil];
    textView.editor = self;
    textView.delegate = self;
    textView.alpha = 0.0;
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    [center addObserver:self selector:@selector(keyboardChanged:) name:UIKeyboardWillChangeFrameNotification object:nil];
    [center addObserver:self selector:@selector(keyboardChanged:) name:UIKeyboardWillHideNotification object:nil];
#endif
    return self;
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

// Applies the keyboard options to a view and returns whether any changed, which a view that already shows its keyboard must reload.
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
    UIView* host = ((__bridge UIWindow*)sapp_ios_get_window()).rootViewController.view;
#if TARGET_OS_TV
    UIView<UITextInput>* view = textField;
#else
    UIView<UITextInput>* view = value.options.keyboard == TextInput::Keyboard::Multiline ? textView : textField;
#endif
    if (view != current && current.isFirstResponder) {
        finishing = YES;
        [current resignFirstResponder];
        finishing = NO;
    }
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
        NSString* text = [NSString stringWithUTF8String:value.text.c_str()];
#if TARGET_OS_TV
        textField.text = text;
#else
        if (view == textView) {
            textView.text = text;
        } else {
            textField.text = text;
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

    if (!view.isFirstResponder) {
        [view becomeFirstResponder];
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
    UIView* host = ((__bridge UIWindow*)sapp_ios_get_window()).rootViewController.view;
    CGRect frame = CGRectZero;
    if (![notification.name isEqualToString:UIKeyboardWillHideNotification] && host.window != nil) {
        const CGRect screen = [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
        frame = CGRectIntersection([host convertRect:screen fromCoordinateSpace:host.window.windowScene.screen.coordinateSpace], host.bounds);
    }
    const haylen::math::Rect area = CGRectIsNull(frame) || CGRectIsEmpty(frame) ? haylen::math::Rect{} : AppleTextInput::toPixels(frame);
    haylen::platform::SokolRuntime::postEvent({.type = haylen::platform::Event::Type::KeyboardChanged, .keyboardFrame = area});
}
#endif

@end
#endif
