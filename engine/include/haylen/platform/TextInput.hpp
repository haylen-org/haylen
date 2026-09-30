#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "haylen/math/Rect.hpp"

namespace haylen::platform {

// Text editing between the UI and the platform. The UI publishes the text field it focuses, and a platform that edits text natively, with a hidden field under the software keyboard and the input methods of the system, sends what the user types back as `TextEdited` and `TextAction` events on the frame thread. Rectangles are in framebuffer pixels and text ranges count code points. Platforms without a native field type through key and character events and keep the defaults of this class.
class TextInput {
  public:
    enum class Keyboard : std::uint8_t {
        Text,
        Multiline,
        Number,
        Decimal,
        Phone,
        Email,
        Url,
        Search,
        Password,
    };

    enum class ReturnKey : std::uint8_t {
        Default,
        Done,
        Go,
        Next,
        Search,
        Send,
    };

    enum class Capitalization : std::uint8_t {
        None,
        Sentences,
        Words,
        Characters,
    };

    // What the user asks for with the return, tab and escape keys, or by closing the keyboard.
    enum class Action : std::uint8_t {
        Submit,
        Next,
        Cancel,
        Dismissed,
    };

    struct Options {
        Keyboard keyboard = Keyboard::Text;
        ReturnKey returnKey = ReturnKey::Default;
        Capitalization capitalization = Capitalization::Sentences;
        bool autocorrect = true;
        int maxLength = 0;

        [[nodiscard]] bool operator==(const Options&) const noexcept = default;
    };

    // A field as the UI shows it. The revision grows whenever the UI itself changes the text or the selection, and a native field replaces its text only when the revision changes.
    struct Field {
        std::uint64_t id = 0;
        std::uint64_t revision = 0;
        std::string text;
        int selectionStart = 0;
        int selectionEnd = 0;
        math::Rect bounds;
        math::Rect caret;
        Options options;

        [[nodiscard]] bool operator==(const Field&) const = default;
    };

    // What a native field holds after the user edited it, with the revision it last received. A composition with a negative start is closed.
    struct Edit {
        std::uint64_t field = 0;
        std::uint64_t revision = 0;
        std::string text;
        int selectionStart = 0;
        int selectionEnd = 0;
        int compositionStart = -1;
        int compositionEnd = -1;

        [[nodiscard]] bool isComposing() const noexcept {
            return compositionStart >= 0 && compositionEnd > compositionStart;
        }
    };

    // The field of the plain on-screen keyboard that `Window::setKeyboardVisible` opens. Its typing reaches the app as key and character events instead of a text field.
    static constexpr std::uint64_t kKeyboardField = 0;

    virtual ~TextInput() = default;

    // Returns whether a native field edits the text, so the UI leaves character and editing keys to it.
    [[nodiscard]] virtual bool isNative() const noexcept {
        return false;
    }

    // Starts editing a field, or follows the field being edited when it moves or changes.
    virtual void edit(const Field&) {}

    // Ends editing and hides the keyboard.
    virtual void finish() {}

    // Lists the text fields on screen, so a platform that opens its keyboard only inside a user gesture can focus its native field as soon as one is tapped.
    virtual void setVisibleFields(std::span<const Field>) {}
};

} // namespace haylen::platform
