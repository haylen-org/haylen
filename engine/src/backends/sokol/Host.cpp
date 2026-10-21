#include "backends/sokol/Host.hpp"

#include <string>

#include "backends/sokol/GpuDevice.hpp"
#include "sokol_glue.h"
#include "sokol_log.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

#include "platform/web/WebPage.hpp"
#elif defined(__ANDROID__)
#include "platform/android/AndroidActivity.hpp"
#include "platform/android/JavaBridge.hpp"
#elif defined(__APPLE__)
#include <TargetConditionals.h>

#include "platform/apple/AppleClipboard.hpp"
#endif

namespace haylen::sokol {

math::Vec2 Host::getFramebufferSize() const noexcept {
    return {sapp_widthf(), sapp_heightf()};
}

// Sokol reports the framebuffer pixels per window pixel on Android, where the activity reports the window pixels per point, the density of the display.
float Host::getDpiScale() const noexcept {
#if defined(__ANDROID__)
    return platform::AndroidActivity::getDensity() * sapp_dpi_scale();
#else
    return sapp_dpi_scale();
#endif
}

bool Host::isFullscreen() const noexcept {
    return sapp_is_fullscreen();
}

void Host::setFullscreen(bool value) {
    if (sapp_is_fullscreen() == value) {
        return;
    }
#if defined(__EMSCRIPTEN__)
    platform::WebPage::setFullscreen(value);
#else
    sapp_toggle_fullscreen();
    // Leaving fullscreen restores the default style of the window, which may have lost its decorations or its fixed size.
    if (!value) {
        applyStyle();
    }
#endif
}

void Host::setTitle(std::string_view value) {
    const std::string text(value);
    sapp_set_window_title(text.c_str());
#if defined(__EMSCRIPTEN__)
    // Sokol only writes the document title while it starts, so later titles reach the page here.
    emscripten_set_window_title(text.c_str());
#endif
}

void Host::setCursor(Cursor value) {
    sapp_set_mouse_cursor(toSokolCursor(value));
}

void Host::setCursorVisible(bool value) {
    sapp_show_mouse(value);
}

void Host::setMouseLocked(bool value) {
    sapp_lock_mouse(value);
}

// The library `sokol_app` keeps the clipboard of iOS, Mac Catalyst and Android inside the app, so the host uses the clipboard of the system there.
void Host::setClipboard(std::string_view value) {
#if defined(__APPLE__) && TARGET_OS_IOS
    platform::AppleClipboard::set(value);
#elif defined(__ANDROID__)
    platform::JavaBridge::setClipboard(value);
#else
    sapp_set_clipboard_string(std::string(value).c_str());
#endif
}

std::string Host::getClipboard() const {
#if defined(__APPLE__) && TARGET_OS_IOS
    return platform::AppleClipboard::get();
#elif defined(__ANDROID__)
    return platform::JavaBridge::getClipboard();
#else
    return sapp_get_clipboard_string();
#endif
}

// The window composes premultiplied alpha with the desktop when it opened transparent, and while it is opaque the frames keep alpha 1.
bool Host::canBeTransparent() const noexcept {
    return sapp_query_desc().composite_mode == SAPP_COMPOSITEMODE_PREMULTIPLIED;
}

std::unique_ptr<gpu::Device> Host::createGpuDevice() {
#if defined(NDEBUG)
    constexpr bool validation = false;
#else
    constexpr bool validation = true;
#endif
    return std::make_unique<GpuDevice>(GpuDevice::Setup{.environment = sglue_environment(), .logger = {.func = slog_func}, .validation = validation, .swapchain = [] { return sglue_swapchain(); }});
}

void Host::closeWindow() {
    sapp_quit();
}

void Host::requestWindowClose() {
    sapp_request_quit();
}

void Host::setWindowFocusable(bool value) {
    sapp_set_window_focusable(value);
}

sapp_mouse_cursor Host::toSokolCursor(Cursor cursor) noexcept {
    switch (cursor) {
    case Cursor::Default:
        return SAPP_MOUSECURSOR_DEFAULT;
    case Cursor::Arrow:
        return SAPP_MOUSECURSOR_ARROW;
    case Cursor::IBeam:
        return SAPP_MOUSECURSOR_IBEAM;
    case Cursor::Crosshair:
        return SAPP_MOUSECURSOR_CROSSHAIR;
    case Cursor::PointingHand:
        return SAPP_MOUSECURSOR_POINTING_HAND;
    case Cursor::ResizeHorizontal:
        return SAPP_MOUSECURSOR_RESIZE_EW;
    case Cursor::ResizeVertical:
        return SAPP_MOUSECURSOR_RESIZE_NS;
    case Cursor::ResizeDiagonalDown:
        return SAPP_MOUSECURSOR_RESIZE_NWSE;
    case Cursor::ResizeDiagonalUp:
        return SAPP_MOUSECURSOR_RESIZE_NESW;
    case Cursor::ResizeAll:
        return SAPP_MOUSECURSOR_RESIZE_ALL;
    case Cursor::NotAllowed:
        return SAPP_MOUSECURSOR_NOT_ALLOWED;
    }
    return SAPP_MOUSECURSOR_DEFAULT;
}

} // namespace haylen::sokol
