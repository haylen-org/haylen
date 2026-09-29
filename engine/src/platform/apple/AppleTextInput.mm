#import "platform/apple/AppleTextInput.hpp"

#include <algorithm>
#include <string>

#include "haylen/platform/Event.hpp"
#include "platform/sokol/SokolRuntime.hpp"
#include "sokol_app.h"

#if TARGET_OS_OSX
#import "platform/apple/HaylenFieldEditor.h"
#else
#import "platform/apple/HaylenTextEditor.h"
#endif

namespace haylen::platform {

#if TARGET_OS_OSX
AppleTextInput::AppleTextInput() : editor([HaylenFieldEditor new]) {}
#else
AppleTextInput::AppleTextInput() : editor([HaylenTextEditor new]) {}
#endif

void AppleTextInput::edit(const Field& field) {
    [editor edit:field];
}

void AppleTextInput::finish() {
    [editor finish];
}

int AppleTextInput::toCodePoints(NSString* text, NSUInteger offset) {
    int count = 0;
    const NSUInteger end = std::min(offset, text.length);
    for (NSUInteger index = 0; index < end; ++count) {
        index += CFStringIsSurrogateHighCharacter([text characterAtIndex:index]) ? 2 : 1;
    }
    return count;
}

NSUInteger AppleTextInput::toUtf16(NSString* text, int codePoints) {
    NSUInteger index = 0;
    for (int count = 0; count < codePoints && index < text.length; ++count) {
        index += CFStringIsSurrogateHighCharacter([text characterAtIndex:index]) ? 2 : 1;
    }
    return std::min(index, text.length);
}

void AppleTextInput::report(std::uint64_t field, std::uint64_t revision, NSString* text, NSRange selection, NSRange marked) {
    const bool composing = marked.location != NSNotFound && marked.length > 0;
    TextInput::Edit edit{.field = field, .revision = revision, .text = std::string(text.UTF8String), .selectionStart = toCodePoints(text, selection.location), .selectionEnd = toCodePoints(text, NSMaxRange(selection))};
    if (composing) {
        edit.compositionStart = toCodePoints(text, marked.location);
        edit.compositionEnd = toCodePoints(text, NSMaxRange(marked));
    }
    SokolRuntime::postEvent({.type = Event::Type::TextEdited, .textEdit = std::move(edit)});
}

void AppleTextInput::act(std::uint64_t field, Action action) {
    SokolRuntime::postEvent({.type = Event::Type::TextAction, .textEdit = {.field = field}, .textAction = action});
}

CGRect AppleTextInput::toPoints(const math::Rect& rect) {
    const CGFloat scale = sapp_dpi_scale();
    return CGRectMake(rect.x / scale, rect.y / scale, rect.width / scale, rect.height / scale);
}

math::Rect AppleTextInput::toPixels(CGRect rect) {
    const auto scale = static_cast<CGFloat>(sapp_dpi_scale());
    return {static_cast<float>(rect.origin.x * scale), static_cast<float>(rect.origin.y * scale), static_cast<float>(rect.size.width * scale), static_cast<float>(rect.size.height * scale)};
}

} // namespace haylen::platform
