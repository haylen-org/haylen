#include "ui/ScrollArbiter.hpp"

#include <cmath>
#include <utility>

#include <imgui.h>

namespace haylen::ui {

void ScrollArbiter::addWheel(math::Vec2 scroll, bool precise) noexcept {
    (precise ? pending.points : pending.lines) += scroll;
}

bool ScrollArbiter::isEmpty(const Wheel& value) noexcept {
    return value.lines.isZero() && value.points.isZero();
}

ImGuiID ScrollArbiter::choose(std::span<const Offer> offers) noexcept {
    const Offer* best = nullptr;
    for (const Offer& offer : offers) {
        if (best == nullptr || (offer.movable && !best->movable) || (offer.movable == best->movable && offer.size < best->size)) {
            best = &offer;
        }
    }
    return best != nullptr ? best->area : 0;
}

void ScrollArbiter::chooseArea(math::Vec2 pointer) {
    const math::Vec2 motion = waiting.lines.isZero() ? waiting.points : waiting.lines;
    const float toward = vertical ? motion.y : motion.x;
    wheelOffers.clear();
    for (const Area& area : previousAreas) {
        if (area.horizontal != vertical && area.hovered && area.maximum > 0.0 && area.bounds.contains(pointer)) {
            wheelOffers.push_back({.area = area.id, .size = area.bounds.width * area.bounds.height, .movable = toward > 0.0F ? area.position > 0.0 : area.position < area.maximum});
        }
    }
    wheelArea = choose(wheelOffers);
}

void ScrollArbiter::beginFrame(float seconds) {
    const ImGuiIO& io = ImGui::GetIO();
    const math::Vec2 pointer{io.MousePos.x, io.MousePos.y};
    wheel = std::exchange(pending, {});
    delivered = wheel;
    areas.swap(previousAreas);
    areas.clear();

    // A wheel that pauses, or that rests while the pointer moves on, ends its gesture, and the next wheel starts one along the axis it moves most.
    const bool scrolled = !isEmpty(wheel);
    if (gesture && (quiet > kGesturePause || (quiet > kPointerPause && (pointer - wheelPointer).getLengthSquared() > kPointerTravel * kPointerTravel))) {
        gesture = false;
    }
    if (scrolled && !gesture) {
        const math::Vec2 motion = wheel.lines.isZero() ? wheel.points : wheel.lines;
        gesture = true;
        vertical = std::fabs(motion.y) >= std::fabs(motion.x);
        waiting = wheel;
        chooseArea(pointer);
        waiting = wheelArea == 0 ? wheel : Wheel{};
    } else if (!isEmpty(waiting)) {
        chooseArea(pointer);
        delivered.lines += waiting.lines;
        delivered.points += waiting.points;
        waiting = {};
    }
    quiet = scrolled ? 0.0F : quiet + seconds;
    if (scrolled) {
        wheelPointer = pointer;
    }

    if (!io.MouseDown[ImGuiMouseButton_Left]) {
        dragArea = 0;
    } else if (dragArea == 0 && !dragOffers.empty()) {
        dragArea = choose(dragOffers);
    }
    dragOffers.clear();
}

ScrollArbiter::AxisWheel ScrollArbiter::takeWheel(ImGuiID area, const math::Rect& bounds, bool horizontal, double position, double maximum, bool hovered) {
    areas.push_back({.id = area, .bounds = bounds, .horizontal = horizontal, .position = position, .maximum = maximum, .hovered = hovered});
    if (!gesture || vertical == horizontal || area != wheelArea) {
        return {};
    }
    return horizontal ? AxisWheel{delivered.lines.x, delivered.points.x} : AxisWheel{delivered.lines.y, delivered.points.y};
}

bool ScrollArbiter::takeDrag(ImGuiID area, const math::Rect& bounds, bool horizontal) {
    if (dragArea != 0) {
        return dragArea == area;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 press = io.MouseClickedPos[ImGuiMouseButton_Left];
    if (!io.MouseDown[ImGuiMouseButton_Left] || !bounds.contains(math::Vec2{press.x, press.y})) {
        return false;
    }
    const ImVec2 moved = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0F);
    const float along = std::fabs(horizontal ? moved.x : moved.y);
    const float across = std::fabs(horizontal ? moved.y : moved.x);
    if (along >= kDragSlop && along > across) {
        dragOffers.push_back({.area = area, .size = bounds.width * bounds.height, .movable = true});
    }
    return false;
}

} // namespace haylen::ui
