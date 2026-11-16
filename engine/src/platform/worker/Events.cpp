#include "platform/worker/Events.hpp"

#include "platform/web/PageModifiers.hpp"
#include "platform/worker/Keys.hpp"

namespace haylen::platform::worker {

std::optional<Event> Events::translateKey(std::string_view code, bool down, bool repeat, int modifiers) noexcept {
    const input::Key key = Keys::fromCode(code);
    if (key == input::Key::Unknown) {
        return std::nullopt;
    }

    return Event{.type = down ? Event::Type::KeyDown : Event::Type::KeyUp, .key = key, .repeat = repeat, .modifiers = PageModifiers::read(modifiers)};
}

// Buttons beyond the three the engine names, such as the back and forward buttons of a mouse, reach the app as nothing.
std::optional<Event> Events::translateMouse(Mouse change, int button, math::Vec2 position, math::Vec2 movement, int modifiers, float density) noexcept {
    const math::Vec2 pixels{position.x * density, position.y * density};
    if (change == Mouse::Move) {
        return Event{.type = Event::Type::MouseMove, .modifiers = PageModifiers::read(modifiers), .position = pixels, .delta = {movement.x * density, movement.y * density}};
    }

    const std::optional<input::MouseButton> pressed = toMouseButton(button);
    if (!pressed) {
        return std::nullopt;
    }

    const Event::Type type = change == Mouse::Down ? Event::Type::MouseDown : Event::Type::MouseUp;
    return Event{.type = type, .modifiers = PageModifiers::read(modifiers), .mouseButton = *pressed, .position = pixels};
}

Event Events::translateScroll(math::Vec2 value, bool precise, int modifiers) noexcept {
    return {.type = Event::Type::MouseScroll, .modifiers = PageModifiers::read(modifiers), .scroll = value, .preciseScroll = precise};
}

std::optional<Event> Events::translateTouch(std::uint64_t id, Touches::Phase phase, math::Vec2 position, int modifiers, float density) {
    return touches.change(id, phase, {position.x * density, position.y * density}, PageModifiers::read(modifiers));
}

std::optional<input::MouseButton> Events::toMouseButton(int button) noexcept {
    switch (button) {
    case 0:
        return input::MouseButton::Left;
    case 1:
        return input::MouseButton::Middle;
    case 2:
        return input::MouseButton::Right;
    default:
        return std::nullopt;
    }
}

} // namespace haylen::platform::worker
