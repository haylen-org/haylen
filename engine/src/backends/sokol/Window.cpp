#include "backends/sokol/Window.hpp"

#include "sokol_app.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace haylen::sokol {

bool Window::queryOpen() const noexcept {
    return sapp_isvalid();
}

void* Window::queryHandle() const noexcept {
#if defined(__APPLE__) && TARGET_OS_OSX
    return const_cast<void*>(sapp_macos_get_window());
#elif defined(__APPLE__)
    return const_cast<void*>(sapp_ios_get_window());
#elif defined(_WIN32)
    return const_cast<void*>(sapp_win32_get_hwnd());
#elif defined(__ANDROID__)
    return const_cast<void*>(sapp_android_get_native_window());
#elif defined(__linux__)
    return const_cast<void*>(sapp_x11_get_window());
#else
    return nullptr;
#endif
}

void* Window::queryDisplay() const noexcept {
#if defined(__linux__) && !defined(__ANDROID__)
    return const_cast<void*>(sapp_x11_get_display());
#else
    return nullptr;
#endif
}

void* Window::queryActivity() const noexcept {
#if defined(__ANDROID__)
    return const_cast<void*>(sapp_android_get_native_activity());
#else
    return nullptr;
#endif
}

math::Vec2 Window::queryFramebufferSize() const noexcept {
    return {sapp_widthf(), sapp_heightf()};
}

float Window::queryScale() const noexcept {
    return sapp_dpi_scale();
}

} // namespace haylen::sokol
