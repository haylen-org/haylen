#include "backends/sokol/Events.hpp"

#include <algorithm>

namespace haylen::sokol {

bool Events::isPlatformBack(const sapp_event& source) noexcept {
    return kRemote && (source.type == SAPP_EVENTTYPE_KEY_DOWN || source.type == SAPP_EVENTTYPE_KEY_UP) && source.key_code == SAPP_KEYCODE_MENU;
}

std::optional<platform::Event> Events::translate(const sapp_event& source) noexcept {
    const input::KeyModifiers held = toModifiers(source.modifiers);
    const bool touch = source.type == SAPP_EVENTTYPE_TOUCHES_BEGAN || source.type == SAPP_EVENTTYPE_TOUCHES_MOVED || source.type == SAPP_EVENTTYPE_TOUCHES_ENDED || source.type == SAPP_EVENTTYPE_TOUCHES_CANCELLED;
    if (kRemote && touch) {
        return std::nullopt;
    }
    switch (source.type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
    case SAPP_EVENTTYPE_KEY_UP:
        // The key codes of `sokol_app` are the numbers of `input::Key`.
        return platform::Event{.type = source.type == SAPP_EVENTTYPE_KEY_DOWN ? platform::Event::Type::KeyDown : platform::Event::Type::KeyUp, .key = isPlatformBack(source) ? input::Key::Escape : static_cast<input::Key>(source.key_code), .repeat = source.key_repeat, .modifiers = held};
    case SAPP_EVENTTYPE_CHAR:
        return platform::Event{.type = platform::Event::Type::Character, .modifiers = held, .character = static_cast<char32_t>(source.char_code)};
    case SAPP_EVENTTYPE_MOUSE_DOWN:
    case SAPP_EVENTTYPE_MOUSE_UP: {
        // Buttons beyond the three the engine names, such as the back and forward buttons of a browser, reach the app as nothing.
        const std::optional<input::MouseButton> button = toMouseButton(source.mouse_button);
        if (!button) {
            return std::nullopt;
        }
        return platform::Event{.type = source.type == SAPP_EVENTTYPE_MOUSE_DOWN ? platform::Event::Type::MouseDown : platform::Event::Type::MouseUp, .modifiers = held, .mouseButton = *button, .position = {source.mouse_x, source.mouse_y}};
    }
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        return platform::Event{.type = platform::Event::Type::MouseMove, .modifiers = held, .position = {source.mouse_x, source.mouse_y}, .delta = {source.mouse_dx, source.mouse_dy}};
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        return platform::Event{.type = platform::Event::Type::MouseScroll, .modifiers = held, .scroll = {source.scroll_x, source.scroll_y}, .preciseScroll = source.scroll_precise, .scrollPhase = toScrollPhase(source.scroll_phase)};
    case SAPP_EVENTTYPE_MOUSE_ENTER:
        return platform::Event{.type = platform::Event::Type::MouseEnter};
    case SAPP_EVENTTYPE_MOUSE_LEAVE:
        return platform::Event{.type = platform::Event::Type::MouseLeave};
    case SAPP_EVENTTYPE_TOUCHES_BEGAN:
        return toTouchEvent(platform::Event::Type::TouchBegan, source);
    case SAPP_EVENTTYPE_TOUCHES_MOVED:
        return toTouchEvent(platform::Event::Type::TouchMoved, source);
    case SAPP_EVENTTYPE_TOUCHES_ENDED:
        return toTouchEvent(platform::Event::Type::TouchEnded, source);
    case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
        return toTouchEvent(platform::Event::Type::TouchCancelled, source);
    case SAPP_EVENTTYPE_RESIZED:
        return platform::Event{.type = platform::Event::Type::Resized};
    case SAPP_EVENTTYPE_ICONIFIED:
    case SAPP_EVENTTYPE_SUSPENDED:
        return platform::Event{.type = platform::Event::Type::Suspended};
    case SAPP_EVENTTYPE_RESTORED:
    case SAPP_EVENTTYPE_RESUMED:
        return platform::Event{.type = platform::Event::Type::Resumed};
    case SAPP_EVENTTYPE_FOCUSED:
        return platform::Event{.type = platform::Event::Type::FocusGained};
    case SAPP_EVENTTYPE_UNFOCUSED:
        return platform::Event{.type = platform::Event::Type::FocusLost};
    case SAPP_EVENTTYPE_QUIT_REQUESTED:
        return platform::Event{.type = platform::Event::Type::QuitRequested};
    default:
        return std::nullopt;
    }
}

input::KeyModifiers Events::toModifiers(std::uint32_t bits) noexcept {
    return {.shift = (bits & SAPP_MODIFIER_SHIFT) != 0U, .control = (bits & SAPP_MODIFIER_CTRL) != 0U, .alt = (bits & SAPP_MODIFIER_ALT) != 0U, .super = (bits & SAPP_MODIFIER_SUPER) != 0U};
}

platform::Event::ScrollPhase Events::toScrollPhase(sapp_scroll_phase phase) noexcept {
    switch (phase) {
    case SAPP_SCROLLPHASE_BEGAN:
        return platform::Event::ScrollPhase::Began;
    case SAPP_SCROLLPHASE_CHANGED:
        return platform::Event::ScrollPhase::Changed;
    case SAPP_SCROLLPHASE_ENDED:
        return platform::Event::ScrollPhase::Ended;
    case SAPP_SCROLLPHASE_MOMENTUM:
        return platform::Event::ScrollPhase::Momentum;
    default:
        return platform::Event::ScrollPhase::None;
    }
}

std::optional<input::MouseButton> Events::toMouseButton(sapp_mousebutton button) noexcept {
    switch (button) {
    case SAPP_MOUSEBUTTON_LEFT:
        return input::MouseButton::Left;
    case SAPP_MOUSEBUTTON_RIGHT:
        return input::MouseButton::Right;
    case SAPP_MOUSEBUTTON_MIDDLE:
        return input::MouseButton::Middle;
    default:
        return std::nullopt;
    }
}

platform::Event Events::toTouchEvent(platform::Event::Type type, const sapp_event& source) noexcept {
    platform::Event event{.type = type, .modifiers = toModifiers(source.modifiers)};
    event.touchCount = std::min<std::size_t>(static_cast<std::size_t>(source.num_touches), platform::Event::kMaxTouchPoints);
    for (std::size_t index = 0; index < event.touchCount; ++index) {
        const sapp_touchpoint& touch = source.touches[index];
        event.touches[index] = {.id = static_cast<std::uint64_t>(touch.identifier), .position = {touch.pos_x, touch.pos_y}, .changed = touch.changed};
    }
    return event;
}

} // namespace haylen::sokol
