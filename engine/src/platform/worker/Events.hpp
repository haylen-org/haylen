#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "haylen/input/MouseButton.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/Touches.hpp"

namespace haylen::platform::worker {

// Turns the input that the page reads over the canvas and posts to the worker into the events of the engine: the keys by their `code`, the mouse and the pen of pointer events, the fingers of touch pointers and the scrolls of wheels and trackpads. Positions arrive in CSS pixels of the canvas box and leave in framebuffer pixels, and the modifiers arrive as the bits of shift, control, alt and the super key, from the lowest.
class Events final {
  public:
    // The changes of a mouse pointer, in the order the page numbers them.
    enum class Mouse : std::uint8_t {
        Down,
        Up,
        Move,
    };

    [[nodiscard]] static std::optional<Event> translateKey(std::string_view code, bool down, bool repeat, int modifiers) noexcept;

    // The button is the one that changed, as the browser numbers it, and the movement is the motion since the last move, which a locked pointer reports alone.
    [[nodiscard]] static std::optional<Event> translateMouse(Mouse change, int button, math::Vec2 position, math::Vec2 movement, int modifiers, float density) noexcept;

    // A precise device, such as a trackpad, scrolls by points of the page, and a wheel by its steps.
    [[nodiscard]] static Event translateScroll(math::Vec2 value, bool precise, int modifiers) noexcept;

    [[nodiscard]] std::optional<Event> translateTouch(std::uint64_t id, Touches::Phase phase, math::Vec2 position, int modifiers, float density);

  private:
    [[nodiscard]] static std::optional<input::MouseButton> toMouseButton(int button) noexcept;

    Touches touches;
};

} // namespace haylen::platform::worker
