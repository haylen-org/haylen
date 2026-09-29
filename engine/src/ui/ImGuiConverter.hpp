#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::ui {

// Converts engine values to Dear ImGui values. Positions are UI coordinates, which are ImGui screen coordinates.
class ImGuiConverter final {
  public:
    [[nodiscard]] static ImVec2 toImVec2(math::Vec2 value) noexcept;
    [[nodiscard]] static ImVec4 toImVec4(math::Color color) noexcept;
    [[nodiscard]] static ImRect toImRect(const math::Rect& rect) noexcept;

    // Converts a color for the current draw list, dimming it while the component is disabled.
    [[nodiscard]] static ImU32 toImU32(math::Color color) noexcept;
};

} // namespace haylen::ui
