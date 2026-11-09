#pragma once

#include <imgui.h>

#include "haylen/math/Rect.hpp"

struct ImGuiWindow;

namespace haylen::ui {
class Backend;
}

namespace haylen::debug {

// Begins and ends the windows of the debug tools with ImGui. A window stays inside the safe area, moves by its title bar and resizes by its corner with the mouse or a finger, and scrolls its content under a finger that drags it, while a tap still reaches its items.
class DebugWindow final {
  public:
    static constexpr float kMargin = 16.0F;
    static constexpr float kMinimumWidth = 240.0F;
    static constexpr float kMinimumHeight = 160.0F;
    static constexpr float kBackgroundAlpha = 0.94F;
    static constexpr ImVec4 kErrorColor{1.0F, 0.54F, 0.52F, 1.0F};

    // Begins a window that first shows over `initial`, an area of UI coordinates, and returns whether its content shows. Every call ends with `end`, whatever it returned.
    static bool begin(const char* title, bool* open, ui::Backend& backend, const math::Rect& initial);
    static void end();

  private:
    static void keepInside(ImGuiWindow& window, const math::Rect& safe);
    static void holdFinger(ImGuiWindow& window, ui::Backend& backend);
    static void followFinger(ImGuiWindow& window);
};

} // namespace haylen::debug
