#pragma once

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>

#include <cstdint>

#include "haylen/platform/TextInput.hpp"

#if TARGET_OS_OSX
@class HaylenFieldEditor;
#else
@class HaylenTextEditor;
#endif

namespace haylen::platform {

// Text input of Apple platforms. A hidden native view edits the focused field: an NSTextView on macOS, which brings marked text, dead keys, the emoji picker and dictation, and a UITextField or UITextView on iOS, tvOS and Mac Catalyst, which brings the software keyboard as well.
class AppleTextInput final : public TextInput {
  public:
    AppleTextInput();

    [[nodiscard]] bool isNative() const noexcept override {
        return true;
    }
    void edit(const Field& field) override;
    void finish() override;

    // Converts between the UTF-16 offsets of Foundation and the code points of the engine.
    [[nodiscard]] static int toCodePoints(NSString* text, NSUInteger offset);
    [[nodiscard]] static NSUInteger toUtf16(NSString* text, int codePoints);

    // Sends what the native view holds, with the marked text of the input method, and its actions to the engine, which receives them on its next frame.
    static void report(std::uint64_t field, std::uint64_t revision, NSString* text, NSRange selection, NSRange marked);
    static void act(std::uint64_t field, Action action);

    // Converts a rectangle between the framebuffer pixels of the engine and the points of the view of the app.
    [[nodiscard]] static CGRect toPoints(const math::Rect& rect);
    [[nodiscard]] static math::Rect toPixels(CGRect rect);

  private:
#if TARGET_OS_OSX
    HaylenFieldEditor* editor;
#else
    HaylenTextEditor* editor;
#endif
};

} // namespace haylen::platform
