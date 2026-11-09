#include "debug/DebugWindow.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

#include "haylen/ui/Backend.hpp"
#include "ui/ScrollArbiter.hpp"

namespace haylen::debug {

bool DebugWindow::begin(const char* title, bool* open, ui::Backend& backend, const math::Rect& initial) {
    const math::Rect safe = backend.getSafeRect();
    ImGui::SetNextWindowPos({initial.x, initial.y}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({initial.width, initial.height}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({std::min(kMinimumWidth, safe.width), std::min(kMinimumHeight, safe.height)}, {safe.width, safe.height});
    ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
    const bool visible = ImGui::Begin(title, open, ImGuiWindowFlags_NoCollapse);

    ImGuiWindow& window = *ImGui::GetCurrentWindow();
    keepInside(window, safe);
    holdFinger(window, backend);
    return visible;
}

void DebugWindow::end() {
    followFinger(*ImGui::GetCurrentWindow());
    ImGui::End();
}

// A window that the player moved, or that a rotation or a smaller screen left outside the safe area, moves back inside it.
void DebugWindow::keepInside(ImGuiWindow& window, const math::Rect& safe) {
    const float x = std::clamp(window.Pos.x, safe.x, std::max(safe.x, safe.x + safe.width - window.Size.x));
    const float y = std::clamp(window.Pos.y, safe.y, std::max(safe.y, safe.y + safe.height - window.Size.y));
    if (x != window.Pos.x || y != window.Pos.y) {
        ImGui::SetWindowPos({x, y});
    }
}

// A finger keeps what it drags on purpose, the title bar, a resize grip or a scroll bar, while a finger that drags the content scrolls it instead of moving the window. Every other item lets go of a finger that travels, as the items of the UI do.
void DebugWindow::holdFinger(ImGuiWindow& window, ui::Backend& backend) {
    const ImGuiContext& state = *ImGui::GetCurrentContext();
    if (state.IO.MouseSource != ImGuiMouseSource_TouchScreen || state.ActiveId == 0 || state.ActiveIdWindow == nullptr || state.ActiveIdWindow->RootWindow != &window) {
        return;
    }

    // The press holds the place on the window where the finger landed, which moves along with the window.
    ImGuiWindow* holder = state.ActiveIdWindow;
    if (state.ActiveId == holder->MoveId) {
        const ImVec2 grabbed{window.Pos.x + state.ActiveIdClickOffset.x, window.Pos.y + state.ActiveIdClickOffset.y};
        if (window.TitleBarRect().Contains(grabbed)) {
            backend.keepDrag(state.ActiveId);
            return;
        }

        // ImGui moved the window by the first motion of the finger before the window began, so it goes back to where the press found it.
        const ImVec2 pressed{state.IO.MouseClickedPos[0].x - state.ActiveIdClickOffset.x, state.IO.MouseClickedPos[0].y - state.ActiveIdClickOffset.y};
        ImGui::ClearActiveID();
        ImGui::SetWindowPos(&window, pressed);
        return;
    }

    const bool scrollbar = state.ActiveId == ImGui::GetWindowScrollbarID(holder, ImGuiAxis_X) || state.ActiveId == ImGui::GetWindowScrollbarID(holder, ImGuiAxis_Y);
    bool grip = false;
    for (int corner = 0; corner < 4; ++corner) {
        grip = grip || state.ActiveId == ImGui::GetWindowResizeCornerID(&window, corner);
    }
    if (scrollbar || grip) {
        backend.keepDrag(state.ActiveId);
    }
}

// Once a finger that pressed the content travels past the slop and nothing holds it, the innermost area under it that scrolls follows it.
void DebugWindow::followFinger(ImGuiWindow& window) {
    const ImGuiContext& state = *ImGui::GetCurrentContext();
    const ImVec2 pressed = state.IO.MouseClickedPos[0];
    if (state.IO.MouseSource != ImGuiMouseSource_TouchScreen || !state.IO.MouseDown[0] || state.ActiveId != 0) {
        return;
    }
    if (!window.Rect().Contains(pressed) || window.TitleBarRect().Contains(pressed)) {
        return;
    }
    const ImVec2 travelled = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0F);
    if (std::hypot(travelled.x, travelled.y) < ui::ScrollArbiter::kDragSlop) {
        return;
    }

    ImGuiWindow* area = state.HoveredWindow;
    while (area != nullptr && area != &window && area->ScrollMax.y <= 0.0F) {
        area = area->ParentWindow;
    }
    if (area == nullptr || area->RootWindow != &window) {
        return;
    }
    ImGui::SetScrollY(area, area->Scroll.y - state.IO.MouseDelta.y);
}

} // namespace haylen::debug
