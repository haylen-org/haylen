#pragma once

#include <imgui.h>

#include <array>
#include <utility>

#include "haylen/math/Rect.hpp"

struct ImGuiWindow;

namespace haylen::ui {
class Backend;
}

namespace haylen::debug {

// Begins and ends the windows of the debug tools with ImGui. Every window has one look of its own whatever the theme of the app, dark and translucent with light text in the default font of the engine, so the tools read the same in every app. A window stays inside the safe area, moves by its title bar and resizes by its corner with the mouse or a finger, and scrolls its content under a finger that drags it, while a tap still reaches its items.
class DebugWindow final {
  public:
    static constexpr float kMargin = 16.0F;
    static constexpr float kMinimumWidth = 240.0F;
    static constexpr float kMinimumHeight = 160.0F;
    static constexpr float kTextSize = 22.0F;
    static constexpr ImVec4 kBackgroundColor{0.043F, 0.059F, 0.094F, 0.9F};
    static constexpr ImVec4 kTextColor{0.91F, 0.94F, 1.0F, 1.0F};
    static constexpr ImVec4 kMutedColor{0.6F, 0.65F, 0.74F, 1.0F};
    static constexpr ImVec4 kAccentColor{0.48F, 0.74F, 1.0F, 1.0F};
    static constexpr ImVec4 kWarningColor{0.97F, 0.8F, 0.44F, 1.0F};
    static constexpr ImVec4 kErrorColor{1.0F, 0.54F, 0.52F, 1.0F};

    // Begins a window that first shows over `initial`, an area of UI coordinates, and returns whether its content shows. Every call ends with `end`, whatever it returned.
    static bool begin(const char* title, bool* open, ui::Backend& backend, const math::Rect& initial);
    static void end();

  private:
    static constexpr float kScrollbarSize = 10.0F;
    static constexpr float kScrollbarGap = 8.0F;
    static constexpr float kScrollbarInset = 4.0F;

    static constexpr std::array<std::pair<ImGuiCol, ImVec4>, 43> kColors{{
        {ImGuiCol_Text, kTextColor}, {ImGuiCol_TextDisabled, kMutedColor}, {ImGuiCol_WindowBg, kBackgroundColor}, {ImGuiCol_ChildBg, {0.0F, 0.0F, 0.0F, 0.0F}}, {ImGuiCol_PopupBg, {0.06F, 0.08F, 0.12F, 0.96F}}, {ImGuiCol_Border, {1.0F, 1.0F, 1.0F, 0.12F}}, {ImGuiCol_BorderShadow, {0.0F, 0.0F, 0.0F, 0.0F}}, {ImGuiCol_FrameBg, {1.0F, 1.0F, 1.0F, 0.11F}}, {ImGuiCol_FrameBgHovered, {1.0F, 1.0F, 1.0F, 0.17F}}, {ImGuiCol_FrameBgActive, {1.0F, 1.0F, 1.0F, 0.23F}}, {ImGuiCol_TitleBg, {0.02F, 0.03F, 0.05F, 0.9F}}, {ImGuiCol_TitleBgActive, {0.07F, 0.1F, 0.16F, 0.94F}}, {ImGuiCol_TitleBgCollapsed, {0.02F, 0.03F, 0.05F, 0.9F}}, {ImGuiCol_ScrollbarBg, {0.0F, 0.0F, 0.0F, 0.0F}}, {ImGuiCol_ScrollbarGrab, {1.0F, 1.0F, 1.0F, 0.2F}}, {ImGuiCol_ScrollbarGrabHovered, {1.0F, 1.0F, 1.0F, 0.3F}}, {ImGuiCol_ScrollbarGrabActive, {1.0F, 1.0F, 1.0F, 0.4F}}, {ImGuiCol_CheckMark, kAccentColor}, {ImGuiCol_SliderGrab, kAccentColor}, {ImGuiCol_SliderGrabActive, {0.66F, 0.84F, 1.0F, 1.0F}}, {ImGuiCol_Button, {1.0F, 1.0F, 1.0F, 0.08F}}, {ImGuiCol_ButtonHovered, {1.0F, 1.0F, 1.0F, 0.14F}}, {ImGuiCol_ButtonActive, {1.0F, 1.0F, 1.0F, 0.2F}}, {ImGuiCol_Header, {1.0F, 1.0F, 1.0F, 0.07F}}, {ImGuiCol_HeaderHovered, {1.0F, 1.0F, 1.0F, 0.12F}}, {ImGuiCol_HeaderActive, {1.0F, 1.0F, 1.0F, 0.16F}}, {ImGuiCol_Separator, {1.0F, 1.0F, 1.0F, 0.12F}}, {ImGuiCol_SeparatorHovered, {0.48F, 0.74F, 1.0F, 0.6F}}, {ImGuiCol_SeparatorActive, kAccentColor}, {ImGuiCol_ResizeGrip, {1.0F, 1.0F, 1.0F, 0.1F}}, {ImGuiCol_ResizeGripHovered, {1.0F, 1.0F, 1.0F, 0.25F}}, {ImGuiCol_ResizeGripActive, {1.0F, 1.0F, 1.0F, 0.4F}}, {ImGuiCol_PlotLines, kAccentColor}, {ImGuiCol_PlotLinesHovered, kWarningColor}, {ImGuiCol_PlotHistogram, kAccentColor}, {ImGuiCol_PlotHistogramHovered, kWarningColor}, {ImGuiCol_TableHeaderBg, {1.0F, 1.0F, 1.0F, 0.06F}}, {ImGuiCol_TableBorderStrong, {1.0F, 1.0F, 1.0F, 0.14F}}, {ImGuiCol_TableBorderLight, {1.0F, 1.0F, 1.0F, 0.08F}}, {ImGuiCol_TableRowBg, {0.0F, 0.0F, 0.0F, 0.0F}}, {ImGuiCol_TableRowBgAlt, {1.0F, 1.0F, 1.0F, 0.03F}}, {ImGuiCol_TextSelectedBg, {0.48F, 0.74F, 1.0F, 0.35F}}, {ImGuiCol_InputTextCursor, kTextColor},
    }};

    static constexpr std::array<std::pair<ImGuiStyleVar, float>, 12> kSizes{{
        {ImGuiStyleVar_Alpha, 1.0F},
        {ImGuiStyleVar_DisabledAlpha, 0.5F},
        {ImGuiStyleVar_WindowRounding, 12.0F},
        {ImGuiStyleVar_WindowBorderSize, 1.0F},
        {ImGuiStyleVar_ChildRounding, 8.0F},
        {ImGuiStyleVar_ChildBorderSize, 1.0F},
        {ImGuiStyleVar_PopupRounding, 8.0F},
        {ImGuiStyleVar_FrameRounding, 6.0F},
        {ImGuiStyleVar_FrameBorderSize, 0.0F},
        {ImGuiStyleVar_IndentSpacing, 24.0F},
        {ImGuiStyleVar_GrabRounding, 6.0F},
        {ImGuiStyleVar_ScrollbarRounding, kScrollbarSize * 0.5F},
    }};

    static constexpr std::array<std::pair<ImGuiStyleVar, ImVec2>, 6> kSpacings{{
        {ImGuiStyleVar_WindowPadding, {16.0F, 14.0F}},
        {ImGuiStyleVar_WindowTitleAlign, {0.0F, 0.5F}},
        {ImGuiStyleVar_FramePadding, {10.0F, 5.0F}},
        {ImGuiStyleVar_ItemSpacing, {12.0F, 8.0F}},
        {ImGuiStyleVar_ItemInnerSpacing, {8.0F, 6.0F}},
        {ImGuiStyleVar_CellPadding, {8.0F, 4.0F}},
    }};

    static void pushStyle(ui::Backend& backend);
    static void popStyle();
    static void keepInside(ImGuiWindow& window, const math::Rect& safe);
    static void holdFinger(ImGuiWindow& window, ui::Backend& backend);
    static void followFinger(ImGuiWindow& window);
};

} // namespace haylen::debug
