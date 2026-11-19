#include "ui/ScrollFinger.hpp"

#include <imgui_internal.h>

#include "haylen/ui/Backend.hpp"
#include "haylen/ui/Context.hpp"
#include "ui/ScrollArbiter.hpp"
#include "ui/Scroller.hpp"

namespace haylen::ui {

bool ScrollFinger::follow(Context& context, Scroller& scroller, ImGuiID id, const math::Rect& bounds, bool horizontal, bool underPointer) {
    ScrollArbiter& arbiter = context.getBackend().getScrollArbiter();
    const ScrollArbiter::Press& press = arbiter.getPress();
    bool held = false;

    // A finger that lands on content that glides stops it where it is and drags it from there, as the content of a touch screen does.
    if (press.began) {
        pressed = underPointer;
        dragging = false;

        if (pressed && press.touch && scroller.isGliding()) {
            scroller.stop();
            arbiter.catchDrag(id);
            held = true;
        }
    }

    if (dragging && press.down) {
        scroller.drag(-static_cast<double>(horizontal ? press.motion.x : press.motion.y), context.getDeltaSeconds());
        return held;
    }

    // A finger that flicks past the slop and lifts before the area took it takes the area as it lifts, so the content still flings.
    if (!dragging && pressed && press.touch && (press.down || press.lifted)) {
        held = take(context, scroller, id, bounds, horizontal) || held;
    }

    if (!press.down) {
        if (dragging) {
            scroller.release();
        }

        pressed = false;
        dragging = false;
    }

    return held;
}

bool ScrollFinger::take(Context& context, Scroller& scroller, ImGuiID id, const math::Rect& bounds, bool horizontal) {
    if (scroller.getMaximum() <= 0.0 || !context.takeDrag(id, bounds, horizontal)) {
        return false;
    }

    if (GImGui->ActiveId != 0 && !ImGui::GetIO().WantTextInput) {
        ImGui::ClearActiveID();
    }

    dragging = true;

    const ScrollArbiter& arbiter = context.getBackend().getScrollArbiter();
    const double travelled = -static_cast<double>(arbiter.getDragTravel(horizontal));
    const double moved = -static_cast<double>(horizontal ? arbiter.getPress().motion.x : arbiter.getPress().motion.y);
    scroller.grab(travelled, moved, context.getDeltaSeconds());
    return true;
}

} // namespace haylen::ui
