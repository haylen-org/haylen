#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "haylen/input/Key.hpp"
#include "haylen/input/KeyModifiers.hpp"
#include "haylen/input/MouseButton.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/platform/TouchPoint.hpp"

namespace haylen::platform {

// Platform event with positions in framebuffer pixels. The engine converts positions to design coordinates.
struct Event {
    enum class Type : std::uint8_t {
        KeyDown,
        KeyUp,
        Character,
        MouseDown,
        MouseUp,
        MouseMove,
        MouseScroll,
        MouseEnter,
        MouseLeave,
        TouchBegan,
        TouchMoved,
        TouchEnded,
        TouchCancelled,
        Resized,
        Suspended,
        Resumed,
        FocusGained,
        FocusLost,
        QuitRequested,
        LowMemory,
        TextEdited,
        TextAction,
        KeyboardChanged,
        NetworkChanged,
        InterruptionBegan,
        InterruptionEnded,
        WindowMoved,
        MonitorsChanged,
    };

    static constexpr std::size_t kMaxTouchPoints = 10;

    Type type = Type::KeyDown;
    input::Key key = input::Key::Unknown;
    bool repeat = false;
    input::KeyModifiers modifiers{};
    char32_t character = 0;
    input::MouseButton mouseButton = input::MouseButton::Left;
    math::Vec2 position{};
    math::Vec2 delta{};
    math::Vec2 scroll{};
    std::array<TouchPoint, kMaxTouchPoints> touches{};
    std::size_t touchCount = 0;

    // A native text field reports its edits in `textEdit`, and its actions in `textAction` for the field in `textEdit.field`.
    TextInput::Edit textEdit;
    TextInput::Action textAction = TextInput::Action::Submit;

    // The area the on-screen keyboard covers, in framebuffer pixels, which is empty while it is hidden.
    math::Rect keyboardFrame;

    // Whether the device reaches the network, as `NetworkChanged` reports it.
    bool online = false;

    // Returns whether the event is player input: keys, text, mouse buttons, movement and scrolling, touches and the edits of native text fields.
    [[nodiscard]] bool isInput() const noexcept {
        switch (type) {
        case Type::KeyDown:
        case Type::KeyUp:
        case Type::Character:
        case Type::MouseDown:
        case Type::MouseUp:
        case Type::MouseMove:
        case Type::MouseScroll:
        case Type::TouchBegan:
        case Type::TouchMoved:
        case Type::TouchEnded:
        case Type::TouchCancelled:
        case Type::TextEdited:
        case Type::TextAction:
            return true;
        default:
            return false;
        }
    }
};

} // namespace haylen::platform
