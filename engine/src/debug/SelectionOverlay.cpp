#include "debug/SelectionOverlay.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>

#include "debug/DebugWindow.hpp"
#include "debug/InspectorWindow.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/ui/Backend.hpp"

namespace haylen::debug {

void SelectionOverlay::draw(Picking& picking, InspectorWindow& inspector, Inspector::Provider& provider, ui::Backend& backend) {
    const math::Rect safe = backend.getSafeRect();
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::SetNextWindowPos({safe.x, safe.y});
    ImGui::SetNextWindowSize({safe.width, safe.height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::Begin(kWindowName.data(), nullptr, flags);
    ImGui::PopStyleVar(2);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    // A press on the surface outside the handles picks what draws under it in the next frame.
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##surface", {safe.width, safe.height});
    if (ImGui::IsItemActivated()) {
        const ImVec2 pressed = ImGui::GetIO().MouseClickedPos[0];
        picking.request(backend.toDesign({pressed.x, pressed.y}));
        drag.reset();
    }

    const std::optional<Picking::Placement>& placement = picking.getPlacement();
    bool held = false;
    if (placement) {
        const ImVec2 min{safe.x, safe.y};
        const ImVec2 max{safe.x + safe.width, safe.y + safe.height};
        const math::Vec2 originPoint = backend.toUi(placement->frame.apply(placement->position));
        const ImVec2 origin{originPoint.x, originPoint.y};
        std::array<ImVec2, 4> outline{};
        float reach = 0.0F;
        for (std::size_t corner = 0; corner < outline.size(); ++corner) {
            const math::Vec2 point = backend.toUi(placement->outline[corner]);
            outline[corner] = {point.x, point.y};
            reach = std::max(reach, (point - originPoint).getLength());
        }

        // The turn handle lies along the first axis of the thing, past its outline.
        const math::Vec2 axis = placement->frame.applyVector({std::cos(placement->rotation), std::sin(placement->rotation)});
        const math::Vec2 direction = axis.isZero() ? math::Vec2{1.0F, 0.0F} : axis / axis.getLength();
        const math::Vec2 turnPoint = originPoint + direction * (reach + kTurnGap);

        ImDrawList& list = *ImGui::GetWindowDrawList();
        const ImU32 accent = ImGui::GetColorU32(DebugWindow::kAccentColor);
        list.AddPolyline(outline.data(), static_cast<int>(outline.size()), accent, kLineWidth, ImDrawFlags_Closed);
        const ImVec2 turn = keepInside({turnPoint.x, turnPoint.y}, min, max);
        list.AddLine(keepInside(origin, min, max), turn, accent, kLineWidth);

        held = drawHandle("##move", keepInside(origin, min, max), Handle::Move, *placement, backend) || held;
        held = drawHandle("##turn", turn, Handle::Turn, *placement, backend) || held;
        held = drawHandle("##scale", keepInside(outline[2], min, max), Handle::Scale, *placement, backend) || held;
    }

    if (!held) {
        drag.reset();
    } else if (drag) {
        const ImVec2 pointer = ImGui::GetIO().MousePos;
        follow(inspector, provider, backend.toDesign({pointer.x, pointer.y}));
    }

    ImGui::End();
}

// The handles keep their look whatever they do: a dot that moves, a ring that turns and a square that scales, lighter while the pointer is over them or holds them.
bool SelectionOverlay::drawHandle(const char* id, ImVec2 center, Handle handle, const Picking::Placement& placement, ui::Backend& backend) {
    ImGui::SetCursorScreenPos({center.x - kHitRadius, center.y - kHitRadius});
    ImGui::InvisibleButton(id, {kHitRadius * 2.0F, kHitRadius * 2.0F});
    if (ImGui::IsItemActivated()) {
        const ImVec2 pressed = ImGui::GetIO().MouseClickedPos[0];
        drag = Drag{.handle = handle, .start = placement, .pointer = backend.toDesign({pressed.x, pressed.y})};
    }

    const bool active = ImGui::IsItemActive() && drag && drag->handle == handle;
    if (active) {
        backend.keepDrag(ImGui::GetItemID());
    }

    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImU32 color = ImGui::GetColorU32(active || ImGui::IsItemHovered() ? DebugWindow::kTextColor : DebugWindow::kAccentColor);
    const ImU32 edge = ImGui::GetColorU32(DebugWindow::kBackgroundColor);
    switch (handle) {
    case Handle::Move:
        list.AddCircleFilled(center, kHandleRadius, color);
        list.AddCircle(center, kHandleRadius, edge, 0, kLineWidth * 0.5F);
        break;
    case Handle::Turn:
        list.AddCircle(center, kHandleRadius, color, 0, kLineWidth * 1.5F);
        break;
    case Handle::Scale:
        list.AddRectFilled({center.x - kHandleRadius, center.y - kHandleRadius}, {center.x + kHandleRadius, center.y + kHandleRadius}, color, 2.0F);
        list.AddRect({center.x - kHandleRadius, center.y - kHandleRadius}, {center.x + kHandleRadius, center.y + kHandleRadius}, edge, 2.0F, kLineWidth * 0.5F);
        break;
    }

    return active;
}

// The pointer moves in the space of the position of the thing, through the inverse of the frame it had when the drag started, so a move follows the pointer one to one on the screen, a turn follows its angle around the position and a scale follows its distance along each axis of the thing.
void SelectionOverlay::follow(InspectorWindow& inspector, Inspector::Provider& provider, math::Vec2 pointer) {
    const Picking::Placement& start = drag->start;
    if (!start.frame.isInvertible()) {
        return;
    }

    const math::Transform2D inverse = start.frame.getInverse();
    const math::Vec2 from = inverse.apply(drag->pointer) - start.position;
    const math::Vec2 to = inverse.apply(pointer) - start.position;

    switch (drag->handle) {
    case Handle::Move: {
        const math::Vec2 position = start.position + (to - from);
        set(inspector, provider, "x", position.x);
        set(inspector, provider, "y", position.y);
        break;
    }
    case Handle::Turn: {
        if (from.getLength() < kMinimumReach || to.getLength() < kMinimumReach) {
            return;
        }

        const float turned = std::atan2(math::Vec2::cross(from, to), math::Vec2::dot(from, to));
        set(inspector, provider, "rotation", start.rotation + turned);
        break;
    }
    case Handle::Scale: {
        const math::Transform2D unturn = math::Transform2D::rotation(-start.rotation);
        const math::Vec2 before = unturn.applyVector(from);
        const math::Vec2 after = unturn.applyVector(to);
        if (std::abs(before.x) >= kMinimumReach) {
            set(inspector, provider, "scaleX", start.scale.x * after.x / before.x);
        }

        if (std::abs(before.y) >= kMinimumReach) {
            set(inspector, provider, "scaleY", start.scale.y * after.y / before.y);
        }

        break;
    }
    }
}

void SelectionOverlay::set(InspectorWindow& inspector, Inspector::Provider& provider, std::string_view key, float value) {
    path[1] = key;
    text.clear();
    std::format_to(std::back_inserter(text), "{}", value);
    inspector.apply(provider, path, text);
}

ImVec2 SelectionOverlay::keepInside(ImVec2 point, ImVec2 min, ImVec2 max) noexcept {
    return {std::clamp(point.x, min.x, std::max(min.x, max.x)), std::clamp(point.y, min.y, std::max(min.y, max.y))};
}

void SelectionOverlay::raise(std::span<const char* const> titles) {
    std::array<ImGuiWindow*, 4> windows{};
    std::size_t count = 0;
    for (const char* title : titles) {
        if (ImGuiWindow* window = ImGui::FindWindowByName(title); window != nullptr && count < windows.size()) {
            windows[count++] = window;
        }
    }

    const auto found = std::span(windows.data(), count);
    std::ranges::sort(found, {}, [](ImGuiWindow* window) { return ImGui::FindWindowDisplayIndex(window); });
    for (ImGuiWindow* window : found) {
        ImGui::BringWindowToDisplayFront(window);
    }
}

} // namespace haylen::debug
