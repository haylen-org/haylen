#pragma once

#include <imgui.h>

#include "haylen/math/Rect.hpp"

namespace haylen::ui {

class Context;
class Scroller;

// The finger that drags the content of an area that scrolls along one axis, as on a touch screen. A finger that lands on content that glides stops it and drags it from there at once, a finger that travels past the slop along the axis takes the area from the control it pressed, a flick that lifts before the area took it still takes it as it lifts, and the content flings on once the finger lets go.
class ScrollFinger final {
  public:
    // Follows the press of the frame over the area `id` inside `bounds` and moves `scroller` with it. Returns whether the finger took hold of the content in this frame, so a scroll that the app asked for ends.
    bool follow(Context& context, Scroller& scroller, ImGuiID id, const math::Rect& bounds, bool horizontal, bool underPointer);

    [[nodiscard]] bool isDragging() const noexcept {
        return dragging;
    }

  private:
    // The content catches up with the finger at once, and a field being edited keeps editing while the finger scrolls the area, while any other item lets the finger go.
    [[nodiscard]] bool take(Context& context, Scroller& scroller, ImGuiID id, const math::Rect& bounds, bool horizontal);

    bool pressed = false;
    bool dragging = false;
};

} // namespace haylen::ui
