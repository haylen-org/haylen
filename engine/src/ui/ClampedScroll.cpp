#include "ui/ClampedScroll.hpp"

#include <algorithm>
#include <cmath>

#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "haylen/ui/ImGuiConverter.hpp"

namespace haylen::ui {

double ClampedScroll::update(Context& context, ImGuiID id, const math::Rect& bounds, double maximum, double view, bool hovered) {
    const float seconds = context.getDeltaSeconds();
    motion.setRange(maximum, view);

    // The steps of a wheel glide, while a trackpad moves the content at once and its momentum goes on after its fingers left, since the area has no fling of its own for them.
    if (maximum > 0.0) {
        const Context::WheelScroll wheel = context.takeWheel(id, bounds, false, motion.getOffset(), maximum, hovered);
        const double moved = static_cast<double>(wheel.distance + wheel.momentum);
        if (wheel.motion == Context::WheelScroll::Motion::Glide) {
            motion.follow(wheel, -1.0, seconds);
        } else if (moved != 0.0) {
            motion.jumpTo(motion.getOffset() - moved);
        }
    }

    static_cast<void>(finger.follow(context, motion, id, bounds, false, hovered));
    motion.update(seconds);
    if (motion.isMoving()) {
        context.requestFrame();
    }

    return std::clamp(motion.getOffset(), 0.0, maximum);
}

void ClampedScroll::updateWindow(Context& context) {
    const ImGuiWindow& window = *ImGui::GetCurrentWindow();
    if (written && !finger.isDragging() && window.Scroll.y != std::min(*written, window.ScrollMax.y)) {
        motion.jumpTo(window.Scroll.y);
    }

    // The window moves after its content drew, so a control that the press made active in this frame still leaves the window under the pointer.
    const math::Rect bounds = ImGuiConverter::toRect(window.InnerRect);
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const double offset = update(context, window.ID, bounds, window.ScrollMax.y, bounds.height, hovered);

    // ImGui keeps the scroll of a window in whole units, and a window that rests keeps a scroll that something else asked for in this frame.
    const float shown = std::round(static_cast<float>(offset));
    written = shown;
    if (shown != window.Scroll.y) {
        ImGui::SetScrollY(shown);
    }
}

} // namespace haylen::ui
