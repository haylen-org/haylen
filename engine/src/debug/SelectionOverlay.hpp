#pragma once

#include <imgui.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "haylen/debug/Inspector.hpp"
#include "haylen/debug/Picking.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::ui {
class Backend;
}

namespace haylen::debug {

class InspectorWindow;

// Draws the surface of the safe area that takes the presses that pick what the app draws, under the windows of the debug tools, and the gizmos of the selection over it in the look of `DebugWindow`: a dot at its position that moves it, a handle beyond its outline that turns it around its position and a handle on the corner of its outline that scales it along its own axes. A drag of a handle sets the position, the rotation and the scale of the root `selected` of the inspector through `InspectorWindow::apply`, the path of every edit of the inspector. Mouse and fingers drive it alike, since the UI hands both to ImGui.
class SelectionOverlay final {
  public:
    static constexpr std::string_view kWindowName = "##selection";
    static constexpr float kHandleRadius = 8.0F;
    static constexpr float kHitRadius = 22.0F;
    static constexpr float kTurnGap = 36.0F;
    static constexpr float kLineWidth = 2.0F;

    // The shortest distance in the space of the position from which a turn or a scale follows the pointer, closer to the position than that the angle and the ratio tell nothing.
    static constexpr float kMinimumReach = 1.0e-4F;

    // Draws the surface and the gizmos of the selection where it drew in this frame, and turns a press on the surface into a pick of the next frame.
    void draw(Picking& picking, InspectorWindow& inspector, Inspector::Provider& provider, ui::Backend& backend);

    // Puts the windows of the debug tools with the titles back over the surface, in the order they had.
    static void raise(std::span<const char* const> titles);

  private:
    enum class Handle : std::uint8_t {
        Move,
        Turn,
        Scale,
    };

    // A drag of a handle, with the placement of the selection and the pointer in design units where it started.
    struct Drag {
        Handle handle = Handle::Move;
        Picking::Placement start;
        math::Vec2 pointer{};
    };

    // Draws a handle at the center and returns whether the pointer holds it, starting its drag when the press lands on it.
    bool drawHandle(const char* id, ImVec2 center, Handle handle, const Picking::Placement& placement, ui::Backend& backend);
    void follow(InspectorWindow& inspector, Inspector::Provider& provider, math::Vec2 pointer);
    void set(InspectorWindow& inspector, Inspector::Provider& provider, std::string_view key, float value);
    [[nodiscard]] static ImVec2 keepInside(ImVec2 point, ImVec2 min, ImVec2 max) noexcept;

    std::optional<Drag> drag;

    // The path of an edit and the text of its value, which keep their storage from edit to edit.
    std::array<std::string, 2> path{"selected", ""};
    std::string text;
};

} // namespace haylen::debug
