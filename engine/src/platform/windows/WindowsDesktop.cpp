#include "platform/windows/WindowsDesktop.hpp"

#include <shellscalingapi.h>

#include <algorithm>
#include <cmath>

#include "haylen/math/Geometry.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/sokol/SokolRuntime.hpp"
#include "sokol_app.h"

namespace haylen::platform {

WNDPROC WindowsDesktop::sokolProcedure = nullptr;
Window::Passthrough WindowsDesktop::passthrough = Window::Passthrough::Off;
std::vector<math::Polygon::Outline> WindowsDesktop::regions;
math::Vec2 WindowsDesktop::pointer{};
bool WindowsDesktop::pointerInside = false;
bool WindowsDesktop::clickThrough = false;

void WindowsDesktop::initialize() {
#if defined(SOKOL_D3D11)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
}

HWND WindowsDesktop::getWindow() {
    return static_cast<HWND>(const_cast<void*>(sapp_win32_get_hwnd()));
}

float WindowsDesktop::getScale(HMONITOR monitor) {
    UINT dpiX = USER_DEFAULT_SCREEN_DPI;
    UINT dpiY = USER_DEFAULT_SCREEN_DPI;
    GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    return static_cast<float>(dpiX) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
}

float WindowsDesktop::getDesktopScale() {
    return getScale(MonitorFromPoint({.x = 0, .y = 0}, MONITOR_DEFAULTTOPRIMARY));
}

// The border and the title bar around a content area depend on the style of the window and the DPI of its monitor.
RECT WindowsDesktop::toOuterFrame(HWND window, RECT content) {
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
    const auto exStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
    AdjustWindowRectExForDpi(&content, style, FALSE, exStyle, GetDpiForWindow(window));
    return content;
}

std::string WindowsDesktop::toUtf8(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(std::max(size, 1)) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    return result;
}

// Borderless windows have no sizing border, so the app resizes them itself.
void WindowsDesktop::setStyle(const WindowStyle& value) {
    const HWND window = getWindow();
    RECT content{};
    GetClientRect(window, &content);
    MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&content), 2);

    constexpr LONG_PTR kKept = WS_VISIBLE | WS_MINIMIZE | WS_MAXIMIZE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    LONG_PTR style = (GetWindowLongPtrW(window, GWL_STYLE) & kKept) | WS_SYSMENU | WS_MINIMIZEBOX;
    if (value.decorated) {
        style |= WS_CAPTION | (value.resizable ? WS_SIZEBOX | WS_MAXIMIZEBOX : 0);
    } else {
        style |= WS_POPUP;
    }

    // A tool window has no taskbar button, and the taskbar notices the change only when the window shows again.
    constexpr LONG_PTR kTaskbar = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW;
    const LONG_PTR previousExStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const LONG_PTR exStyle = (previousExStyle & ~kTaskbar) | (value.shownInTaskbar ? WS_EX_APPWINDOW | WS_EX_WINDOWEDGE : WS_EX_TOOLWINDOW);
    SetWindowLongPtrW(window, GWL_STYLE, style);
    SetWindowLongPtrW(window, GWL_EXSTYLE, exStyle);

    // The content area stays where it was, and the outer frame follows the new border, while a minimized window keeps the frame it restores to.
    const RECT frame = toOuterFrame(window, content);
    const UINT keep = IsIconic(window) != FALSE ? SWP_NOMOVE | SWP_NOSIZE : 0;
    SetWindowPos(window, value.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top, SWP_NOACTIVATE | SWP_FRAMECHANGED | keep);
    if ((exStyle & kTaskbar) != (previousExStyle & kTaskbar) && IsWindowVisible(window)) {
        ShowWindow(window, SW_HIDE);
        ShowWindow(window, SW_SHOWNA);
    }
}

math::Rect WindowsDesktop::getFrame() {
    const HWND window = getWindow();
    RECT content{};
    GetClientRect(window, &content);
    MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&content), 2);
    const float scale = getDesktopScale();
    return {static_cast<float>(content.left) / scale, static_cast<float>(content.top) / scale, static_cast<float>(content.right - content.left) / scale, static_cast<float>(content.bottom - content.top) / scale};
}

void WindowsDesktop::setFrame(const math::Rect& value) {
    const HWND window = getWindow();
    const float scale = getDesktopScale();
    const RECT content{.left = std::lround(value.x * scale), .top = std::lround(value.y * scale), .right = std::lround(value.getRight() * scale), .bottom = std::lround(value.getBottom() * scale)};
    const RECT frame = toOuterFrame(window, content);
    SetWindowPos(window, nullptr, frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL CALLBACK WindowsDesktop::addMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM list) {
    MonitorList& found = *reinterpret_cast<MonitorList*>(list);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) == FALSE) {
        return TRUE;
    }
    const float scale = found.desktopScale;
    const auto toPoints = [scale](const RECT& area) { return math::Rect{static_cast<float>(area.left) / scale, static_cast<float>(area.top) / scale, static_cast<float>(area.right - area.left) / scale, static_cast<float>(area.bottom - area.top) / scale}; };
    found.monitors.push_back({.name = toUtf8(info.szDevice), .bounds = toPoints(info.rcMonitor), .workArea = toPoints(info.rcWork), .scale = getScale(monitor), .primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0});
    return TRUE;
}

// The work area of a monitor leaves out the taskbar and the bars that dock to its edges.
std::vector<Monitor> WindowsDesktop::getMonitors() {
    MonitorList list{.desktopScale = getDesktopScale()};
    EnumDisplayMonitors(nullptr, nullptr, &addMonitor, reinterpret_cast<LPARAM>(&list));
    return list.monitors;
}

// Only a layered window lets clicks through, and a layered window shows nothing until it has an opacity.
void WindowsDesktop::setPassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> value) {
    passthrough = mode;
    regions.assign(value.begin(), value.end());
    const HWND window = getWindow();
    const LONG_PTR exStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if (mode == Window::Passthrough::Off) {
        SetWindowLongPtrW(window, GWL_EXSTYLE, exStyle & ~(WS_EX_LAYERED | WS_EX_TRANSPARENT));
        clickThrough = false;
        return;
    }
    if ((exStyle & WS_EX_LAYERED) == 0) {
        SetWindowLongPtrW(window, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        SetLayeredWindowAttributes(window, 0, 255, LWA_ALPHA);
    }
}

void WindowsDesktop::setClickThrough(bool value) {
    if (value == clickThrough) {
        return;
    }
    clickThrough = value;
    const HWND window = getWindow();
    const LONG_PTR exStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    SetWindowLongPtrW(window, GWL_EXSTYLE, value ? exStyle | WS_EX_TRANSPARENT : exStyle & ~WS_EX_TRANSPARENT);
}

// The move loop of the system runs the frames of the app from a timer of sokol_app, so the drag starts from the window procedure between frames.
void WindowsDesktop::startDrag() {
    PostMessageW(getWindow(), kStartDragMessage, 0, 0);
}

void WindowsDesktop::watch() {
    const HWND window = getWindow();
    sokolProcedure = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windowProcedure)));
}

void WindowsDesktop::update() {
    if (passthrough == Window::Passthrough::Off) {
        return;
    }
    const HWND window = getWindow();
    POINT cursor{};
    GetCursorPos(&cursor);
    ScreenToClient(window, &cursor);
    RECT client{};
    GetClientRect(window, &client);
    const bool inside = PtInRect(&client, cursor) != FALSE;
    const float scale = client.right > 0 ? sapp_widthf() / static_cast<float>(client.right) : 1.0F;
    const math::Vec2 pixel{static_cast<float>(cursor.x) * scale, static_cast<float>(cursor.y) * scale};

    // The window keeps the mouse it has while a button is down, so a press that starts over the app also ends there.
    const bool pressed = GetAsyncKeyState(VK_LBUTTON) < 0 || GetAsyncKeyState(VK_RBUTTON) < 0 || GetAsyncKeyState(VK_MBUTTON) < 0;
    if (!pressed) {
        setClickThrough(passthrough == Window::Passthrough::Whole || !inside || !isOverRegion(pixel));
    }

    // A window that lets clicks through hears no mouse moves either, so the app hears them from here.
    if (clickThrough) {
        followPointer(pixel, inside);
    }
}

bool WindowsDesktop::isOverRegion(math::Vec2 pixel) {
    return std::ranges::any_of(regions, [pixel](const math::Polygon::Outline& region) { return math::Geometry::contains(region, pixel); });
}

void WindowsDesktop::followPointer(math::Vec2 pixel, bool inside) {
    if (inside && (!pointerInside || pixel != pointer)) {
        SokolRuntime::handleEvent({.type = Event::Type::MouseMove, .position = pixel});
    } else if (!inside && pointerInside) {
        SokolRuntime::handleEvent({.type = Event::Type::MouseLeave, .position = pixel});
    }
    pointer = pixel;
    pointerInside = inside;
}

LRESULT CALLBACK WindowsDesktop::windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case kStartDragMessage: {
        if (GetAsyncKeyState(VK_LBUTTON) >= 0) {
            return 0;
        }
        ReleaseCapture();
        SendMessageW(window, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);

        // The move loop keeps the release of the button, which sokol_app and the app still need to hear.
        POINT cursor{};
        GetCursorPos(&cursor);
        ScreenToClient(window, &cursor);
        PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(static_cast<WORD>(cursor.x), static_cast<WORD>(cursor.y)));
        return 0;
    }
    case WM_MOVE:
        SokolRuntime::postEvent({.type = Event::Type::WindowMoved});
        break;
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        SokolRuntime::postEvent({.type = Event::Type::MonitorsChanged});
        break;
    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETWORKAREA) {
            SokolRuntime::postEvent({.type = Event::Type::MonitorsChanged});
        }
        break;
    default:
        break;
    }
    return CallWindowProcW(sokolProcedure, window, message, wParam, lParam);
}

} // namespace haylen::platform
