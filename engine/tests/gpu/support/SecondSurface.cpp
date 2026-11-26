#include "gpu/support/SecondSurface.hpp"

#include <cstdint>
#include <stdexcept>

#include "gpu/DeviceFactory.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <X11/Xlib.h>
#elif defined(__EMSCRIPTEN__)
#include <string>

#include <emscripten/emscripten.h>

// clang-format off
EM_JS(void, haylen_js_add_test_canvas, (const char* name, int width, int height), {
    const canvas = document.createElement("canvas");
    canvas.id = UTF8ToString(name);
    canvas.width = width;
    canvas.height = height;
    document.body.appendChild(canvas);
});

EM_JS(void, haylen_js_remove_test_canvas, (const char* name), {
    document.getElementById(UTF8ToString(name)).remove();
});

EM_JS(int, haylen_js_is_headless_chrome_on_linux, (), {
    const agent = navigator.userAgent;
    return agent.includes("HeadlessChrome/") && agent.includes("Linux") && !agent.includes("Android") ? 1 : 0;
});
// clang-format on
#endif

namespace haylen::test {

// The native window of the surface, which the platform opens without showing it: a window of Windows that is never shown, or a window of X11 that is never mapped.
class SecondSurface::Native final {
  public:
#if defined(_WIN32)
    Native(int width, int height, const gpu::Device&) {
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
    Native(int width, int height, const gpu::Device&) : display(openDisplay()) {
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
        XSync(display, False);
    }

    // The connection stays open until the process ends, as the one of the window of an app does. A Vulkan driver may hook the closing of a display it drew to, such as the one of NVIDIA, and its code leaves with the last Vulkan instance, so a display closed after the device would call into it.
    static Display* openDisplay() {
        static Display* const shared = XOpenDisplay(nullptr);
        return shared;
    }

    [[nodiscard]] gpu::Surface describe() const noexcept {
        return {.kind = gpu::Surface::Kind::Xlib, .window = reinterpret_cast<void*>(static_cast<std::uintptr_t>(window)), .display = display};
    }

  private:
    Display* display = nullptr;
    Window window = 0;
#elif defined(__EMSCRIPTEN__)
    // A canvas of the page that the tests add for the surface and take away after it, which WebGPU configures with the device of the window.
    Native(int width, int height, const gpu::Device& device) : name("haylen-second-surface-" + std::to_string(nextCanvas++)), selector("#" + name) {
        if (haylen_js_is_headless_chrome_on_linux() == 1 && device.getAdapterName().find("swiftshader") != std::string::npos) {
            throw std::runtime_error("Headless Chrome on Linux draws WebGPU through SwiftShader, which cannot share the swapchain of a canvas with the compositor of the browser, so the browser takes the device away from a page that presents a canvas and the page has no surface beyond the window.");
        }

        haylen_js_add_test_canvas(name.c_str(), width, height);
    }

    Native(const Native&) = delete;
    Native& operator=(const Native&) = delete;

    ~Native() {
        haylen_js_remove_test_canvas(name.c_str());
    }

    [[nodiscard]] gpu::Surface describe() const noexcept {
        return {.kind = gpu::Surface::Kind::WebCanvas, .canvas = selector};
    }

  private:
    static inline int nextCanvas = 1;
    std::string name;
    std::string selector;
#else
    Native(int, int, const gpu::Device&) {
        throw std::runtime_error("The tests open no window on this platform for a surface beyond the window.");
    }

    [[nodiscard]] static gpu::Surface describe() noexcept {
        return {};
    }
#endif
};

SecondSurface::SecondSurface(const gpu::Device& device, OffscreenSurface& offscreen, int width, int height) {
    if (gpu::DeviceFactory::usesGlContext(device.getApi())) {
        drawable = offscreen.makeDrawable(width, height);
        surface = {.kind = gpu::Surface::Kind::Gl, .gl = drawable.get()};
    } else {
        native = std::make_unique<Native>(width, height, device);
        surface = native->describe();
    }

    surface.size = {.width = width, .height = height, .windowWidth = width, .windowHeight = height};
}

SecondSurface::~SecondSurface() = default;

} // namespace haylen::test
