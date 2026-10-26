#include "platform/android/AndroidInputEvents.hpp"

#include <android/input.h>
#include <android/keycodes.h>

#include <algorithm>
#include <cstddef>

#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"
#include "platform/android/AndroidGamepads.hpp"
#include "platform/android/AndroidMouseWheel.hpp"

namespace haylen::platform {

bool AndroidInputEvents::wantsKey(const GameActivityKeyEvent& event) {
    return toKey(event.keyCode) != input::Key::Unknown || typesCharacter(event) || AndroidGamepads::takesKey(event);
}

// A hardware keyboard types through the characters of its key events.
void AndroidInputEvents::dispatchKey(const GameActivityKeyEvent& event) {
    if (AndroidGamepads::handleKey(event)) {
        return;
    }

    const bool pressed = event.action == AKEY_EVENT_ACTION_DOWN;
    const bool repeat = event.repeatCount > 0;
    const input::KeyModifiers modifiers = toModifiers(event.metaState);
    if (const input::Key key = toKey(event.keyCode); key != input::Key::Unknown) {
        Player::handleEvent({.type = pressed ? Event::Type::KeyDown : Event::Type::KeyUp, .key = key, .repeat = repeat, .modifiers = modifiers});
    }
    if (pressed && typesCharacter(event)) {
        Player::handleEvent({.type = Event::Type::Character, .repeat = repeat, .modifiers = modifiers, .character = static_cast<char32_t>(event.unicodeChar)});
    }
}

void AndroidInputEvents::dispatchMotion(const GameActivityMotionEvent& event) {
    if (AndroidGamepads::handleMotion(event) || AndroidMouseWheel::handleEvent(event)) {
        return;
    }
    dispatchTouches(event);
}

// Fingers, pens and the buttons of mice all reach the app as touches, at positions that the scale of the window turns into framebuffer pixels. A finger that goes down or up while others stay is the only one that changed.
void AndroidInputEvents::dispatchTouches(const GameActivityMotionEvent& event) {
    const std::int32_t action = event.action & AMOTION_EVENT_ACTION_MASK;
    const std::optional<Event::Type> type = toTouchType(action);
    if ((event.source & AINPUT_SOURCE_CLASS_POINTER) == 0 || !type) {
        return;
    }

    const bool single = action == AMOTION_EVENT_ACTION_POINTER_DOWN || action == AMOTION_EVENT_ACTION_POINTER_UP;
    const auto changedIndex = static_cast<std::size_t>((event.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    const float scale = NativeWindow::getScale();
    Event touches{.type = *type, .modifiers = toModifiers(event.metaState)};
    touches.touchCount = std::min<std::size_t>(event.pointerCount, Event::kMaxTouchPoints);
    for (std::size_t index = 0; index < touches.touchCount; ++index) {
        const GameActivityPointerAxes& pointer = event.pointers[index];
        const math::Vec2 position{GameActivityPointerAxes_getX(&pointer) * scale, GameActivityPointerAxes_getY(&pointer) * scale};
        touches.touches[index] = {.id = static_cast<std::uint64_t>(pointer.id), .position = position, .changed = !single || index == changedIndex};
    }
    Player::handleEvent(touches);
}

bool AndroidInputEvents::typesCharacter(const GameActivityKeyEvent& event) noexcept {
    return event.unicodeChar >= 32 && event.unicodeChar != 127;
}

// The center of a directional pad confirms like enter, and the play and pause key of TV remotes pauses.
input::Key AndroidInputEvents::toKey(std::int32_t code) noexcept {
    if (code >= AKEYCODE_A && code <= AKEYCODE_Z) {
        return static_cast<input::Key>(static_cast<int>(input::Key::A) + code - AKEYCODE_A);
    }
    if (code >= AKEYCODE_0 && code <= AKEYCODE_9) {
        return static_cast<input::Key>(static_cast<int>(input::Key::Digit0) + code - AKEYCODE_0);
    }
    if (code >= AKEYCODE_NUMPAD_0 && code <= AKEYCODE_NUMPAD_9) {
        return static_cast<input::Key>(static_cast<int>(input::Key::Keypad0) + code - AKEYCODE_NUMPAD_0);
    }
    if (code >= AKEYCODE_F1 && code <= AKEYCODE_F12) {
        return static_cast<input::Key>(static_cast<int>(input::Key::F1) + code - AKEYCODE_F1);
    }

    switch (code) {
    case AKEYCODE_SPACE:
        return input::Key::Space;
    case AKEYCODE_APOSTROPHE:
        return input::Key::Apostrophe;
    case AKEYCODE_COMMA:
        return input::Key::Comma;
    case AKEYCODE_MINUS:
        return input::Key::Minus;
    case AKEYCODE_PERIOD:
        return input::Key::Period;
    case AKEYCODE_SLASH:
        return input::Key::Slash;
    case AKEYCODE_SEMICOLON:
        return input::Key::Semicolon;
    case AKEYCODE_EQUALS:
        return input::Key::Equal;
    case AKEYCODE_LEFT_BRACKET:
        return input::Key::LeftBracket;
    case AKEYCODE_BACKSLASH:
        return input::Key::Backslash;
    case AKEYCODE_RIGHT_BRACKET:
        return input::Key::RightBracket;
    case AKEYCODE_GRAVE:
        return input::Key::GraveAccent;
    case AKEYCODE_ESCAPE:
        return input::Key::Escape;
    case AKEYCODE_ENTER:
    case AKEYCODE_DPAD_CENTER:
        return input::Key::Enter;
    case AKEYCODE_TAB:
        return input::Key::Tab;
    case AKEYCODE_DEL:
        return input::Key::Backspace;
    case AKEYCODE_INSERT:
        return input::Key::Insert;
    case AKEYCODE_FORWARD_DEL:
        return input::Key::Delete;
    case AKEYCODE_DPAD_RIGHT:
        return input::Key::Right;
    case AKEYCODE_DPAD_LEFT:
        return input::Key::Left;
    case AKEYCODE_DPAD_DOWN:
        return input::Key::Down;
    case AKEYCODE_DPAD_UP:
        return input::Key::Up;
    case AKEYCODE_PAGE_UP:
        return input::Key::PageUp;
    case AKEYCODE_PAGE_DOWN:
        return input::Key::PageDown;
    case AKEYCODE_MOVE_HOME:
        return input::Key::Home;
    case AKEYCODE_MOVE_END:
        return input::Key::End;
    case AKEYCODE_CAPS_LOCK:
        return input::Key::CapsLock;
    case AKEYCODE_SCROLL_LOCK:
        return input::Key::ScrollLock;
    case AKEYCODE_NUM_LOCK:
        return input::Key::NumLock;
    case AKEYCODE_SYSRQ:
        return input::Key::PrintScreen;
    case AKEYCODE_BREAK:
    case AKEYCODE_MEDIA_PLAY_PAUSE:
        return input::Key::Pause;
    case AKEYCODE_NUMPAD_DOT:
        return input::Key::KeypadDecimal;
    case AKEYCODE_NUMPAD_DIVIDE:
        return input::Key::KeypadDivide;
    case AKEYCODE_NUMPAD_MULTIPLY:
        return input::Key::KeypadMultiply;
    case AKEYCODE_NUMPAD_SUBTRACT:
        return input::Key::KeypadSubtract;
    case AKEYCODE_NUMPAD_ADD:
        return input::Key::KeypadAdd;
    case AKEYCODE_NUMPAD_ENTER:
        return input::Key::KeypadEnter;
    case AKEYCODE_NUMPAD_EQUALS:
        return input::Key::KeypadEqual;
    case AKEYCODE_SHIFT_LEFT:
        return input::Key::LeftShift;
    case AKEYCODE_CTRL_LEFT:
        return input::Key::LeftControl;
    case AKEYCODE_ALT_LEFT:
        return input::Key::LeftAlt;
    case AKEYCODE_META_LEFT:
        return input::Key::LeftSuper;
    case AKEYCODE_SHIFT_RIGHT:
        return input::Key::RightShift;
    case AKEYCODE_CTRL_RIGHT:
        return input::Key::RightControl;
    case AKEYCODE_ALT_RIGHT:
        return input::Key::RightAlt;
    case AKEYCODE_META_RIGHT:
        return input::Key::RightSuper;
    case AKEYCODE_MENU:
        return input::Key::Menu;
    default:
        return input::Key::Unknown;
    }
}

input::KeyModifiers AndroidInputEvents::toModifiers(std::int32_t metaState) noexcept {
    return {.shift = (metaState & AMETA_SHIFT_ON) != 0, .control = (metaState & AMETA_CTRL_ON) != 0, .alt = (metaState & AMETA_ALT_ON) != 0, .super = (metaState & AMETA_META_ON) != 0};
}

std::optional<Event::Type> AndroidInputEvents::toTouchType(std::int32_t action) noexcept {
    switch (action) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
        return Event::Type::TouchBegan;
    case AMOTION_EVENT_ACTION_MOVE:
        return Event::Type::TouchMoved;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
        return Event::Type::TouchEnded;
    case AMOTION_EVENT_ACTION_CANCEL:
        return Event::Type::TouchCancelled;
    default:
        return std::nullopt;
    }
}

} // namespace haylen::platform
