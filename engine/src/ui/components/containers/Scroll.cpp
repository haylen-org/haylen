#include "ui/components/containers/Scroll.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

void Scroll::readProperties(PropertyReader& reader) {
    std::string axis = horizontal ? "horizontal" : "vertical";
    reader.read("scrollbar", scrollbarShown);
    reader.read("axis", axis);
    reader.read("snap", snap);
    if (axis != "vertical" && axis != "horizontal") {
        reader.fail("axis", "must be \"vertical\" or \"horizontal\"");
    }
    horizontal = axis == "horizontal";
}

// A horizontal scroll adds the lane of its bar to its height while its child is wider than it, so the child keeps the height it measures.
math::Vec2 Scroll::measureContent(Context& context, float availableWidth) {
    auto visible = getLayoutChildren();
    if (visible.empty()) {
        return {};
    }
    const math::Vec2 size = visible.front()->measure(context, horizontal ? CommonProperties::kUnbounded : availableWidth);
    const float lane = horizontal && scrollbarShown && size.x > availableWidth ? Scrollbar::getLane(context) : 0.0F;
    return {std::min(size.x, availableWidth), size.y + lane};
}

void Scroll::render(Context& context, const math::Rect& bounds) {
    // ImGui gives a child without a width or a height the rest of its window, so an empty scroll draws nothing instead.
    if (bounds.isEmpty()) {
        return;
    }
    auto visible = getLayoutChildren();
    Component* child = visible.empty() ? nullptr : visible.front();

    // The bar takes its lane while the child at the whole width is longer than the area, so the content never moves back and forth as the bar comes and goes.
    math::Vec2 size = child != nullptr ? child->measure(context, horizontal ? CommonProperties::kUnbounded : bounds.width) : math::Vec2{};
    const bool overflowing = scrollbarShown && (horizontal ? size.x > bounds.width : size.y > bounds.height);
    const math::Rect box = overflowing ? Scrollbar::getContentBox(context, bounds, bounds, horizontal) : bounds;
    if (overflowing && !horizontal) {
        size = child->measure(context, box.width);
    }

    ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(bounds.getMin()));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar;
    if (horizontal) {
        flags |= ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    }
    if (ImGui::BeginChild("##scroll", ImGuiConverter::toImVec2(bounds.getSize()), ImGuiChildFlags_NavFlattened, flags)) {
        // The bar takes the pointer in its lane before the content does.
        if (overflowing) {
            const float view = horizontal ? bounds.width : bounds.height;
            const double maximum = std::max(0.0F, (horizontal ? size.x : size.y) - view);
            const std::optional<double> offset = scrollbar.interact(context, bounds, horizontal, false, horizontal ? ImGui::GetScrollX() : ImGui::GetScrollY(), maximum, view);
            if (offset && horizontal) {
                ImGui::SetScrollX(static_cast<float>(*offset));
            } else if (offset) {
                ImGui::SetScrollY(static_cast<float>(*offset));
            }
        }

        // The child lays out in the box the lane leaves, scrolled, and draws only inside it.
        points.clear();
        if (child != nullptr) {
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const math::Vec2 origin = math::Vec2{start.x, start.y} + (box.getMin() - bounds.getMin());
            const math::Rect area = horizontal ? math::Rect{origin.x, origin.y, size.x, box.height} : math::Rect{origin.x, origin.y, box.width, size.y};
            ImGui::PushClipRect(ImGuiConverter::toImVec2(box.getMin()), ImGuiConverter::toImVec2(box.getMax()), true);
            child->draw(context, area);
            ImGui::PopClipRect();
            ImGui::SetCursorScreenPos(start);
            ImGui::Dummy({area.getRight() - start.x, area.getBottom() - start.y});

            // The items of the child are the places snapping settles on, measured from the start of the content.
            for (const auto& item : child->getChildren()) {
                if (item->getCommon().visible && !item->getBounds().isEmpty()) {
                    points.push_back(horizontal ? item->getBounds().x - origin.x : item->getBounds().y - origin.y);
                }
            }
        }

        const ImGuiIO& io = ImGui::GetIO();
        const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        if (horizontal && hovered && io.MouseWheel != 0.0F) {
            ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseWheel * context.getMetric(Theme::Metric::ControlHeight));
        }
        followFinger();
        const bool touched = (hovered && (io.MouseWheel != 0.0F || io.MouseWheelH != 0.0F || ImGui::IsMouseDown(ImGuiMouseButton_Left))) || ImGui::GetActiveID() != 0;
        idle = touched ? 0.0F : idle + context.getDeltaSeconds();
        if (snap && idle > kSnapDelay) {
            settle(context, horizontal ? ImGui::GetScrollX() : ImGui::GetScrollY(), horizontal ? ImGui::GetScrollMaxX() : ImGui::GetScrollMaxY());
        }
        if (overflowing) {
            scrollbar.draw(context, 1.0F);
        }
    }
    ImGui::EndChild();
}

void Scroll::followFinger() {
    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiContext& state = *GImGui;
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) || io.MouseSource != ImGuiMouseSource_TouchScreen || !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        return;
    }

    // Only a drag along the scrolling axis takes the finger from a control, and a text field being edited keeps it.
    const ImVec2 dragged = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
    const bool along = horizontal ? std::fabs(dragged.x) > std::fabs(dragged.y) : std::fabs(dragged.y) > std::fabs(dragged.x);
    if (state.ActiveId != 0 && (!along || io.WantTextInput || state.ActiveIdWindow == nullptr || state.ActiveIdWindow->RootWindow != ImGui::GetCurrentWindow()->RootWindow)) {
        return;
    }
    if (state.ActiveId != 0) {
        ImGui::ClearActiveID();
    }
    if (horizontal) {
        ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x);
    } else {
        ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
    }
}

void Scroll::settle(Context& context, float position, float limit) {
    if (points.empty()) {
        return;
    }
    float target = std::clamp(points.front(), 0.0F, limit);
    for (const float point : points) {
        const float candidate = std::clamp(point, 0.0F, limit);
        if (std::fabs(candidate - position) < std::fabs(target - position)) {
            target = candidate;
        }
    }
    // ImGui rounds scroll positions, so every step moves at least one pixel and never stalls short of the target.
    const float distance = target - position;
    const float step = std::max(std::fabs(distance) * std::min(1.0F, context.getDeltaSeconds() * kSnapSpeed), 1.0F);
    const float next = std::fabs(distance) <= step ? target : position + std::copysign(step, distance);
    if (horizontal) {
        ImGui::SetScrollX(next);
    } else {
        ImGui::SetScrollY(next);
    }
}

} // namespace haylen::ui
