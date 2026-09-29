#include "platform/sokol/SokolEvents.hpp"

#include <algorithm>

namespace haylen::platform {

bool SokolEvents::isPlatformBack(const sapp_event& source) noexcept {
    return kRemote && (source.type == SAPP_EVENTTYPE_KEY_DOWN || source.type == SAPP_EVENTTYPE_KEY_UP) && source.key_code == SAPP_KEYCODE_MENU;
}

std::optional<Event> SokolEvents::translate(const sapp_event& source) noexcept {
    const input::KeyModifiers held = toModifiers(source.modifiers);
    const bool touch = source.type == SAPP_EVENTTYPE_TOUCHES_BEGAN || source.type == SAPP_EVENTTYPE_TOUCHES_MOVED || source.type == SAPP_EVENTTYPE_TOUCHES_ENDED || source.type == SAPP_EVENTTYPE_TOUCHES_CANCELLED;
    if (kRemote && touch) {
        return std::nullopt;
    }
    switch (source.type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
    case SAPP_EVENTTYPE_KEY_UP:
        // Sokol key codes follow the GLFW values that the engine keys use as well.
        return Event{.type = source.type == SAPP_EVENTTYPE_KEY_DOWN ? Event::Type::KeyDown : Event::Type::KeyUp, .key = isPlatformBack(source) ? input::Key::Escape : static_cast<input::Key>(source.key_code), .repeat = source.key_repeat, .modifiers = held};
    case SAPP_EVENTTYPE_CHAR:
        return Event{.type = Event::Type::Character, .modifiers = held, .character = static_cast<char32_t>(source.char_code)};
    case SAPP_EVENTTYPE_MOUSE_DOWN:
    case SAPP_EVENTTYPE_MOUSE_UP:
        return Event{.type = source.type == SAPP_EVENTTYPE_MOUSE_DOWN ? Event::Type::MouseDown : Event::Type::MouseUp, .modifiers = held, .mouseButton = toMouseButton(source.mouse_button), .position = {source.mouse_x, source.mouse_y}};
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        return Event{.type = Event::Type::MouseMove, .modifiers = held, .position = {source.mouse_x, source.mouse_y}, .delta = {source.mouse_dx, source.mouse_dy}};
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        return Event{.type = Event::Type::MouseScroll, .modifiers = held, .scroll = {source.scroll_x, source.scroll_y}};
    case SAPP_EVENTTYPE_MOUSE_ENTER:
        return Event{.type = Event::Type::MouseEnter};
    case SAPP_EVENTTYPE_MOUSE_LEAVE:
        return Event{.type = Event::Type::MouseLeave};
    case SAPP_EVENTTYPE_TOUCHES_BEGAN:
        return toTouchEvent(Event::Type::TouchBegan, source);
    case SAPP_EVENTTYPE_TOUCHES_MOVED:
        return toTouchEvent(Event::Type::TouchMoved, source);
    case SAPP_EVENTTYPE_TOUCHES_ENDED:
        return toTouchEvent(Event::Type::TouchEnded, source);
    case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
        return toTouchEvent(Event::Type::TouchCancelled, source);
    case SAPP_EVENTTYPE_RESIZED:
        return Event{.type = Event::Type::Resized};
    case SAPP_EVENTTYPE_ICONIFIED:
    case SAPP_EVENTTYPE_SUSPENDED:
        return Event{.type = Event::Type::Suspended};
    case SAPP_EVENTTYPE_RESTORED:
    case SAPP_EVENTTYPE_RESUMED:
        return Event{.type = Event::Type::Resumed};
    case SAPP_EVENTTYPE_FOCUSED:
        return Event{.type = Event::Type::FocusGained};
    case SAPP_EVENTTYPE_UNFOCUSED:
        return Event{.type = Event::Type::FocusLost};
    case SAPP_EVENTTYPE_QUIT_REQUESTED:
        return Event{.type = Event::Type::QuitRequested};
    default:
        return std::nullopt;
    }
}

input::KeyModifiers SokolEvents::toModifiers(std::uint32_t bits) noexcept {
    return {.shift = (bits & SAPP_MODIFIER_SHIFT) != 0U, .control = (bits & SAPP_MODIFIER_CTRL) != 0U, .alt = (bits & SAPP_MODIFIER_ALT) != 0U, .super = (bits & SAPP_MODIFIER_SUPER) != 0U};
}

input::MouseButton SokolEvents::toMouseButton(sapp_mousebutton button) noexcept {
    switch (button) {
    case SAPP_MOUSEBUTTON_RIGHT:
        return input::MouseButton::Right;
    case SAPP_MOUSEBUTTON_MIDDLE:
        return input::MouseButton::Middle;
    default:
        return input::MouseButton::Left;
    }
}

Event SokolEvents::toTouchEvent(Event::Type type, const sapp_event& source) noexcept {
    Event event{.type = type, .modifiers = toModifiers(source.modifiers)};
    event.touchCount = std::min<std::size_t>(static_cast<std::size_t>(source.num_touches), Event::kMaxTouchPoints);
    for (std::size_t index = 0; index < event.touchCount; ++index) {
        const sapp_touchpoint& touch = source.touches[index];
        event.touches[index] = {.id = static_cast<std::uint64_t>(touch.identifier), .position = {touch.pos_x, touch.pos_y}, .changed = touch.changed};
    }
    return event;
}

} // namespace haylen::platform
