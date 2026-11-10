#include "gpu/support/SecondSurface.hpp"

#include <cstdint>
#include <stdexcept>

#include "gpu/DeviceFactory.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <X11/Xlib.h>
#endif

namespace haylen::test {

// The native window of the surface, which the platform opens without showing it: a window of Windows that is never shown, or a window of X11 that is never mapped.
class SecondSurface::Native final {
  public:
#if defined(_WIN32)
    Native(int width, int height) {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = L"HaylenSecondSurface";
        RegisterClassW(&windowClass);
        window = CreateWindowExW(0, windowClass.lpszClassName, L"Haylen", WS_OVERLAPPEDWINDOW, 0, 0, width, height, nullptr, nullptr, instance, nullptr);
        if (window == nullptr) {
            throw std::runtime_error("Windows could not open the hidden window of the surface.");
        }
    }

    Native(const Native&) = delete;
    Native& operator=(const Native&) = delete;

    ~Native() {
        DestroyWindow(window);
    }

    [[nodiscard]] gpu::Surface describe() const noexcept {
        return {.kind = gpu::Surface::Kind::Win32, .window = window, .display = GetModuleHandleW(nullptr)};
    }

  private:
    HWND window = nullptr;
#elif defined(__linux__) && !defined(__ANDROID__)
    Native(int width, int height) : display(XOpenDisplay(nullptr)) {
        if (display == nullptr) {
            throw std::runtime_error("No display server of X11 runs, so the tests have no window for the surface.");
        }
        window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, static_cast<unsigned int>(width), static_cast<unsigned int>(height), 0, 0, 0);
        XSync(display, False);
    }

    Native(const Native&) = delete;
    Native& operator=(const Native&) = delete;

    ~Native() {
        XDestroyWindow(display, window);
        XCloseDisplay(display);
    }

    [[nodiscard]] gpu::Surface describe() const noexcept {
        return {.kind = gpu::Surface::Kind::Xlib, .window = reinterpret_cast<void*>(static_cast<std::uintptr_t>(window)), .display = display};
    }

  private:
    Display* display = nullptr;
    Window window = 0;
#else
    Native(int, int) {
        throw std::runtime_error("The tests open no window on this platform for a surface beyond the window.");
    }

    [[nodiscard]] static gpu::Surface describe() noexcept {
        return {};
    }
#endif
};

SecondSurface::SecondSurface(gpu::Device::Api api, OffscreenSurface& offscreen, int width, int height) {
    if (gpu::DeviceFactory::usesGlContext(api)) {
        drawable = offscreen.makeDrawable(width, height);
        surface = {.kind = gpu::Surface::Kind::Gl, .gl = drawable.get()};
    } else {
        native = std::make_unique<Native>(width, height);
        surface = native->describe();
    }
    surface.size = {.width = width, .height = height, .windowWidth = width, .windowHeight = height};
}

SecondSurface::~SecondSurface() = default;

} // namespace haylen::test
