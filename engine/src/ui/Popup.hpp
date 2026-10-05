#pragma once

#include <optional>

#include <imgui.h>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "ui/Scrollbar.hpp"

namespace haylen::ui {

class Context;

// The window of a menu, a popover or a picker on the theme `menu` surface over its shadow, next to the control that opened it. It takes the size of its content a padding away from its edges and stays inside the display, and content taller than the room it has scrolls beside a scroll bar at the edge of the popup, which grows by the part of the lane of the bar that its padding leaves.
class Popup final {
  public:
    // Opens the popup when it is open and returns the box its content lays out in, scrolled, which is as large as the content or a little narrower beside the bar, or nothing while it is closed. The popup hangs below the anchor, or above it when the room below is too short for the content and the room above is larger, lined up with the side of the anchor where the UI starts and as wide as the anchor and `minimumWidth` at least. An open popup ends with `end`.
    [[nodiscard]] std::optional<math::Rect> begin(Context& context, const char* name, ImGuiWindowFlags flags, float padding, math::Vec2 content, const math::Rect& anchor, float minimumWidth);
    void end(Context& context);

  private:
    static constexpr float kAnchorGap = 4.0F;

    Scrollbar scrollbar;
    math::Rect box;
    bool overflowing = false;
};

} // namespace haylen::ui
