#include "ui/ScrollArbiter.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <imgui.h>

namespace haylen::ui {

// The momentum of a trackpad counts apart, the fingers that touch it and leave it count once per frame, and the scroll they make while they move counts with the points of the frame.
void ScrollArbiter::addWheel(math::Vec2 scroll, bool precise, platform::Event::ScrollPhase phase) noexcept {
    using Phase = platform::Event::ScrollPhase;
    if (phase == Phase::Momentum) {
        pending.momentum += scroll;
        return;
    }
    pendingTouched = pendingTouched || phase == Phase::Began;
    pendingLifted = pendingLifted || phase == Phase::Ended;
    pendingPhased = pendingPhased || phase != Phase::None;
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

// An item near the start or the end of an area scrolls it that way, faster the closer it gets, up to full speed at the edge and past it, and among the areas it nears that can move that way the smallest one scrolls, so an area inside another wins over it.
void ScrollArbiter::chooseEdge(math::Vec2 pointer) {
    edgeArea = 0;
    edgeFactor = 0.0F;
    float smallest = 0.0F;
    for (const EdgeArea& area : previousEdgeAreas) {
        const float along = area.horizontal ? pointer.x - area.bounds.x : pointer.y - area.bounds.y;
        const float across = area.horizontal ? pointer.y - area.bounds.y : pointer.x - area.bounds.x;
        const float length = area.horizontal ? area.bounds.width : area.bounds.height;
        const float breadth = area.horizontal ? area.bounds.height : area.bounds.width;
        if (area.edge <= 0.0F || across < 0.0F || across > breadth || along < -area.edge || along > length + area.edge) {
            continue;
        }

        float factor = 0.0F;
        if (along < area.edge && area.position > 0.0) {
            factor = -(1.0F - std::max(along, 0.0F) / area.edge);
        } else if (along > length - area.edge && area.position < area.maximum) {
            factor = 1.0F - std::max(length - along, 0.0F) / area.edge;
        }
        const float size = area.bounds.width * area.bounds.height;
        if (factor != 0.0F && (edgeArea == 0 || size < smallest)) {
            edgeArea = area.id;
            edgeFactor = factor;
            smallest = size;
        }
    }
}

void ScrollArbiter::beginFrame(float seconds) {
    const ImGuiIO& io = ImGui::GetIO();
    const math::Vec2 pointer{io.MousePos.x, io.MousePos.y};
    frameSeconds = seconds;
    edgeAreas.swap(previousEdgeAreas);
    edgeAreas.clear();
    itemDragged = std::exchange(carrying, false) || ImGui::GetDragDropPayload() != nullptr;
    if (itemDragged) {
        chooseEdge(pointer);
    } else {
        edgeArea = 0;
    }
    wheel = std::exchange(pending, {});
    delivered = wheel;
    touched = std::exchange(pendingTouched, false);
    lifted = std::exchange(pendingLifted, false);
    const bool framePhased = std::exchange(pendingPhased, false);
    areas.swap(previousAreas);
    areas.clear();

    // A gesture of a trackpad with phases lasts while its fingers are on it or its momentum goes on. A wheel without phases that pauses, or that rests while the pointer moves on, ends its gesture. The next wheel starts a gesture along the axis it moves most.
    const bool scrolled = !isEmpty(wheel);
    if (gesture && !fingers && ((phased && wheel.momentum.isZero()) || quiet > kGesturePause || (quiet > kPointerPause && (pointer - wheelPointer).getLengthSquared() > kPointerTravel * kPointerTravel))) {
        gesture = false;
    }
    fingers = (fingers || touched || (framePhased && scrolled)) && !lifted;
    if (scrolled && !gesture) {
        const math::Vec2 motion = wheel.lines.isZero() ? wheel.points : wheel.lines;
        gesture = true;
        phased = framePhased;
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

    // An item that the finger picked up in the frame the drag offers came keeps the finger.
    if (!io.MouseDown[ImGuiMouseButton_Left]) {
        dragArea = 0;
    } else if (dragArea == 0 && !dragOffers.empty() && !itemDragged) {
        dragArea = choose(dragOffers);
    }
    dragOffers.clear();
}

ScrollArbiter::AxisWheel ScrollArbiter::takeWheel(ImGuiID area, const math::Rect& bounds, bool horizontal, double position, double maximum, bool hovered) {
    areas.push_back({.id = area, .bounds = bounds, .horizontal = horizontal, .position = position, .maximum = maximum, .hovered = hovered});
    const ImVec2 pointer = ImGui::GetIO().MousePos;
    AxisWheel result{.touched = touched && hovered && bounds.contains(math::Vec2{pointer.x, pointer.y})};
    if (!gesture || vertical == horizontal || area != wheelArea) {
        return result;
    }
    result.lines = horizontal ? delivered.lines.x : delivered.lines.y;
    result.points = horizontal ? delivered.points.x : delivered.points.y;
    result.momentum = horizontal ? delivered.momentum.x : delivered.momentum.y;
    result.held = phased && (fingers || lifted);
    result.lifted = phased && lifted;
    return result;
}

bool ScrollArbiter::takeDrag(ImGuiID area, const math::Rect& bounds, bool horizontal) {
    if (dragArea != 0) {
        return dragArea == area;
    }
    if (itemDragged) {
        return false;
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

float ScrollArbiter::takeEdgeScroll(ImGuiID area, const math::Rect& bounds, bool horizontal, double position, double maximum, float edge) {
    edgeAreas.push_back({.id = area, .bounds = bounds, .horizontal = horizontal, .position = position, .maximum = maximum, .edge = edge});
    return area == edgeArea ? edgeFactor * kEdgeSpeed * frameSeconds : 0.0F;
}

} // namespace haylen::ui
