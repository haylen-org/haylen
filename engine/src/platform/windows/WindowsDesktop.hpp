#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Display.hpp"
#include "haylen/platform/Window.hpp"
#include "platform/WindowStyle.hpp"

namespace haylen::platform {

// The desktop window of Windows. Desktop points are pixels divided by the scale of the primary display, which sizes the window of `app.json` too. Only a layered window lets clicks through, so passthrough follows the mouse every frame and makes the window transparent to clicks outside the regions, while the app keeps hearing the mouse move. The window procedure of SDL gets a procedure in front of it that reports moves, display changes, the app theme and the power status, and starts drags between frames.
class WindowsDesktop final {
  public:
    static void setStyle(const WindowStyle& value);
    [[nodiscard]] static math::Rect getFrame();
    static void setFrame(const math::Rect& value);
    [[nodiscard]] static std::vector<Display> getDisplays();
    [[nodiscard]] static std::string getWindowDisplayId();
    static void setPassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> value);
    static void startDrag();
    static void watch();
    static void update();

  private:
    static constexpr UINT kStartDragMessage = WM_APP + 1;

    // The power setting `GUID_POWER_SAVING_STATUS`, which tells whether the battery saver is on.
    static constexpr GUID kPowerSavingStatus{0xe00958c0, 0xc213, 0x4ace, {0xac, 0x77, 0xfe, 0xcc, 0xed, 0x2e, 0xee, 0xa5}};

    struct DisplayList {
        std::vector<Display> displays;
        float desktopScale = 1.0F;
    };

    // The id and the name of a monitor.
    struct Identity {
        std::string id;
        std::string name;
    };

    static WNDPROC sdlProcedure;
    static Window::Passthrough passthrough;
    static std::vector<math::Polygon::Outline>& regions;
    static math::Vec2 pointer;
    static bool pointerInside;
    static bool clickThrough;

    [[nodiscard]] static HWND getWindow();
    [[nodiscard]] static float getScale(HMONITOR monitor);
    [[nodiscard]] static float getDesktopScale();
    [[nodiscard]] static RECT toOuterFrame(HWND window, RECT content);
    [[nodiscard]] static bool isOverRegion(math::Vec2 pixel);
    static void setClickThrough(bool value);
    static void followPointer(math::Vec2 pixel, bool inside);
    [[nodiscard]] static Identity identify(std::wstring_view device);
    [[nodiscard]] static std::vector<Display::Mode> getModes(const wchar_t* device, float scale);
    static BOOL CALLBACK addDisplay(HMONITOR monitor, HDC context, LPRECT area, LPARAM list);
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
};

} // namespace haylen::platform
