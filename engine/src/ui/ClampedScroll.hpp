#pragma once

#include <optional>

#include <imgui.h>

#include "haylen/math/Rect.hpp"
#include "ui/ScrollFinger.hpp"
#include "ui/Scroller.hpp"

namespace haylen::ui {

class Context;

// The vertical scroll of an area whose content never shows past its ends, such as the lines of a text area, a popup and the body of a dialog. The steps of a wheel glide to their place, a trackpad moves the content at once and the momentum that the system adds once its fingers left moves it on, and a finger drags the content and flings it.
class ClampedScroll final {
  public:
    // Moves the content of the area `id` inside `bounds` by the input of the frame and returns how far it shows from its top, out of `maximum`, for a view `view` long.
    double update(Context& context, ImGuiID id, const math::Rect& bounds, double maximum, double view, bool hovered);

    // Moves the content of the current ImGui window, which ImGui places, by the input of the frame from the next frame on, and follows a scroll that something else gave the window, such as the focus bringing a control into view. Windows that scroll this way set `ImGuiWindowFlags_NoScrollWithMouse`.
    void updateWindow(Context& context);

    // Jumps to an offset that something else asks for, such as the caret of a field or a scroll bar.
    void jumpTo(double offset) noexcept {
        motion.jumpTo(offset);
    }

    [[nodiscard]] double getOffset() const noexcept {
        return motion.getOffset();
    }

  private:
    Scroller motion;
    ScrollFinger finger;
    std::optional<float> written;
};

} // namespace haylen::ui
