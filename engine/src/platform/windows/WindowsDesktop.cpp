#include "platform/windows/WindowsDesktop.hpp"

#include <shellscalingapi.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Geometry.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"
#include "platform/windows/WindowsAccessibility.hpp"
#include "platform/windows/WindowsSystem.hpp"
#include "platform/windows/WindowsText.hpp"

namespace haylen::platform {

WNDPROC WindowsDesktop::sdlProcedure = nullptr;
Window::Passthrough WindowsDesktop::passthrough = Window::Passthrough::Off;
std::vector<math::Polygon::Outline>& WindowsDesktop::regions = *new std::vector<math::Polygon::Outline>();
math::Vec2 WindowsDesktop::pointer{};
bool WindowsDesktop::pointerInside = false;
bool WindowsDesktop::clickThrough = false;

HWND WindowsDesktop::getWindow() {
    return static_cast<HWND>(NativeWindow::getHandle());
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

// The paths of the display configuration join the source that a monitor of the desktop draws from to the monitor itself, whose device path stays the same across runs and reconnections and whose friendly name the settings of Windows show. A source without a path, such as the display of a remote session, keeps the name of its device as its id, and a monitor without a friendly name keeps it as its name.
WindowsDesktop::Identity WindowsDesktop::identify(std::wstring_view device) {
    Identity identity{.id = WindowsText::toUtf8(device), .name = WindowsText::toUtf8(device)};
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
        return identity;
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS) {
        return identity;
    }

    for (UINT32 index = 0; index < pathCount; ++index) {
        const DISPLAYCONFIG_PATH_INFO& path = paths[index];
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header = {.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, .size = static_cast<UINT32>(sizeof(source)), .adapterId = path.sourceInfo.adapterId, .id = path.sourceInfo.id};
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS || device != source.viewGdiDeviceName) {
            continue;
        }
        DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
        target.header = {.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME, .size = static_cast<UINT32>(sizeof(target)), .adapterId = path.targetInfo.adapterId, .id = path.targetInfo.id};
        if (DisplayConfigGetDeviceInfo(&target.header) != ERROR_SUCCESS) {
            continue;
        }
        identity.id = WindowsText::toUtf8(target.monitorDevicePath);
        if (target.monitorFriendlyDeviceName[0] != L'\0') {
            identity.name = WindowsText::toUtf8(target.monitorFriendlyDeviceName);
        }
        return identity;
    }
    return identity;
}

// The modes of a monitor in true color, each size and rate once.
std::vector<Display::Mode> WindowsDesktop::getModes(const wchar_t* device, float scale) {
    std::vector<Display::Mode> modes;
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    for (DWORD index = 0; EnumDisplaySettingsW(device, index, &mode) != FALSE; ++index) {
        const Display::Mode found{.width = static_cast<int>(mode.dmPelsWidth), .height = static_cast<int>(mode.dmPelsHeight), .refreshRate = static_cast<float>(mode.dmDisplayFrequency), .scale = scale};
        if (mode.dmBitsPerPel >= 24 && std::ranges::find(modes, found) == modes.end()) {
            modes.push_back(found);
        }
    }
    return modes;
}

// A rate of 0 or 1 is the default rate of the hardware, which Windows does not tell.
BOOL CALLBACK WindowsDesktop::addDisplay(HMONITOR monitor, HDC, LPRECT, LPARAM list) {
    DisplayList& found = *reinterpret_cast<DisplayList*>(list);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) == FALSE) {
        return TRUE;
    }
    DEVMODEW current{};
    current.dmSize = sizeof(current);
    const float rate = EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &current) != FALSE && current.dmDisplayFrequency > 1 ? static_cast<float>(current.dmDisplayFrequency) : 0.0F;
    const float scale = found.desktopScale;
    const auto toPoints = [scale](const RECT& area) { return math::Rect{static_cast<float>(area.left) / scale, static_cast<float>(area.top) / scale, static_cast<float>(area.right - area.left) / scale, static_cast<float>(area.bottom - area.top) / scale}; };
    Identity identity = identify(info.szDevice);
    const float monitorScale = getScale(monitor);
    found.displays.push_back({.id = std::move(identity.id), .name = std::move(identity.name), .bounds = toPoints(info.rcMonitor), .workArea = toPoints(info.rcWork), .scale = monitorScale, .refreshRate = rate, .primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0, .modes = getModes(info.szDevice, monitorScale)});
    return TRUE;
}

// The work area of a monitor leaves out the taskbar and the bars that dock to its edges.
std::vector<Display> WindowsDesktop::getDisplays() {
    DisplayList list{.desktopScale = getDesktopScale()};
    EnumDisplayMonitors(nullptr, nullptr, &addDisplay, reinterpret_cast<LPARAM>(&list));
    return list.displays;
}

std::string WindowsDesktop::getWindowDisplayId() {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(MonitorFromWindow(getWindow(), MONITOR_DEFAULTTOPRIMARY), &info);
    return identify(info.szDevice).id;
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

// The move loop of the system runs the frames of the app from a timer of SDL, so the drag starts from the window procedure between frames.
void WindowsDesktop::startDrag() {
    PostMessageW(getWindow(), kStartDragMessage, 0, 0);
}

// The battery saver announces itself as a power setting, which reaches only the windows that register for it, for as long as the window lives.
void WindowsDesktop::watch() {
    const HWND window = getWindow();
    sdlProcedure = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windowProcedure)));
    RegisterPowerSettingNotification(window, &kPowerSavingStatus, DEVICE_NOTIFY_WINDOW_HANDLE);
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
    const float scale = client.right > 0 ? NativeWindow::getFramebufferSize().x / static_cast<float>(client.right) : 1.0F;
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
        Player::handleEvent({.type = Event::Type::MouseMove, .position = pixel});
    } else if (!inside && pointerInside) {
        Player::handleEvent({.type = Event::Type::MouseLeave, .position = pixel});
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

        // The move loop keeps the release of the button, which the backend and the app still need to hear.
        POINT cursor{};
        GetCursorPos(&cursor);
        ScreenToClient(window, &cursor);
        PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(static_cast<WORD>(cursor.x), static_cast<WORD>(cursor.y)));
        return 0;
    }
    case WM_MOVE:
        Player::postEvent({.type = Event::Type::WindowMoved});
        break;
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        Player::postEvent({.type = Event::Type::DisplaysChanged});
        break;
    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETWORKAREA) {
            Player::postEvent({.type = Event::Type::DisplaysChanged});
        }
        // Windows names the colors of the personalization settings, the app theme among them, as the immersive color set.
        if (lParam != 0 && std::wstring_view(reinterpret_cast<const wchar_t*>(lParam)) == L"ImmersiveColorSet") {
            WindowsSystem::reportTheme();
        }
        // Windows names the regional and language settings as the international settings.
        if (lParam != 0 && std::wstring_view(reinterpret_cast<const wchar_t*>(lParam)) == L"intl") {
            WindowsSystem::reportLanguages();
        }
        // The accessibility settings change with several of the settings that Windows announces, so every announcement reads them again.
        WindowsSystem::reportAccessibility();
        break;
    case WM_GETOBJECT:
        if (const std::optional<LRESULT> answered = WindowsAccessibility::answer(window, wParam, lParam)) {
            return *answered;
        }
        break;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMPOWERSTATUSCHANGE || wParam == PBT_POWERSETTINGCHANGE) {
            WindowsSystem::reportPowerStatus();
        }
        break;
    case WM_TIMECHANGE:
        WindowsSystem::reportTimeZone();
        break;
    default:
        break;
    }
    return CallWindowProcW(sdlProcedure, window, message, wParam, lParam);
}

} // namespace haylen::platform
