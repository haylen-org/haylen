#include "ui/components/containers/Scroll.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/ui/Context.hpp"
#include "ui/ImGuiConverter.hpp"

namespace haylen::ui {

void Scroll::readProperties(PropertyReader& reader) {
    std::string axis = horizontal ? "horizontal" : "vertical";
    reader.read("scrollbar", scrollbar);
    reader.read("axis", axis);
    reader.read("snap", snap);
    if (axis != "vertical" && axis != "horizontal") {
        reader.fail("axis", "must be vertical or horizontal");
    }
    horizontal = axis == "horizontal";
}

math::Vec2 Scroll::measureContent(Context& context, float availableWidth) {
    const std::vector<Component*> visible = getLayoutChildren();
    if (visible.empty()) {
        return {};
    }
    const math::Vec2 size = visible.front()->measure(context, horizontal ? std::numeric_limits<float>::max() : availableWidth);
    return {std::min(size.x, availableWidth), size.y};
}

void Scroll::render(Context& context, const math::Rect& bounds) {
    ImGui::SetCursorScreenPos(ImGuiConverter::toImVec2(bounds.getMin()));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavInputs;
    if (!scrollbar) {
        flags |= ImGuiWindowFlags_NoScrollbar;
    }
    if (horizontal) {
        flags |= ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    }
    if (ImGui::BeginChild("##scroll", ImGuiConverter::toImVec2(bounds.getSize()), ImGuiChildFlags_NavFlattened, flags)) {
        points.clear();
        const std::vector<Component*> visible = getLayoutChildren();
        if (!visible.empty()) {
            Component& child = *visible.front();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const math::Vec2 size = child.measure(context, horizontal ? std::numeric_limits<float>::max() : available.x);
            const math::Rect area = horizontal ? math::Rect{origin.x, origin.y, size.x, available.y} : math::Rect{origin.x, origin.y, available.x, size.y};
            child.draw(context, area);
            ImGui::SetCursorScreenPos(origin);
            ImGui::Dummy(ImGuiConverter::toImVec2(area.getSize()));

            // The items of the child are the places snapping settles on, measured from the start of the content.
            for (const auto& item : child.getChildren()) {
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
