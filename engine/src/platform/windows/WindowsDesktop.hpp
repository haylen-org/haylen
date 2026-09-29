#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <span>
#include <string>
#include <vector>

#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Monitor.hpp"
#include "haylen/platform/Window.hpp"
#include "platform/WindowStyle.hpp"

namespace haylen::platform {

// The desktop window of Windows. Desktop points are pixels divided by the scale of the primary monitor, which sizes the window of app.json too. Only a layered window lets clicks through, so passthrough follows the mouse every frame and makes the window transparent to clicks outside the regions, while the app keeps hearing the mouse move. The window procedure of sokol_app gets a procedure in front of it that reports moves and monitor changes and starts drags between frames.
class WindowsDesktop final {
  public:
    // Makes the process aware of the DPI of each monitor, as sokol_app makes D3D11 apps when their window opens, so the monitors read before the window exists have the same coordinates as the window.
    static void initialize();

    static void setStyle(const WindowStyle& value);
    [[nodiscard]] static math::Rect getFrame();
    static void setFrame(const math::Rect& value);
    [[nodiscard]] static std::vector<Monitor> getMonitors();
    static void setPassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> value);
    static void startDrag();
    static void watch();
    static void update();

  private:
    static constexpr UINT kStartDragMessage = WM_APP + 1;

    struct MonitorList {
        std::vector<Monitor> monitors;
        float desktopScale = 1.0F;
    };

    static WNDPROC sokolProcedure;
    static Window::Passthrough passthrough;
    static std::vector<math::Polygon::Outline>& regions;
    static math::Vec2 pointer;
    static bool pointerInside;
    static bool clickThrough;

    [[nodiscard]] static HWND getWindow();
    [[nodiscard]] static float getScale(HMONITOR monitor);
    [[nodiscard]] static float getDesktopScale();
    [[nodiscard]] static RECT toOuterFrame(HWND window, RECT content);
    [[nodiscard]] static std::string toUtf8(const wchar_t* text);
    [[nodiscard]] static bool isOverRegion(math::Vec2 pixel);
    static void setClickThrough(bool value);
    static void followPointer(math::Vec2 pixel, bool inside);
    static BOOL CALLBACK addMonitor(HMONITOR monitor, HDC context, LPRECT area, LPARAM list);
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
};

} // namespace haylen::platform
