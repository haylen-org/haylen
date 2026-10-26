#pragma once

#include <string_view>

namespace haylen::gpu {

// The context of OpenGL, OpenGL ES or WebGL2 that the platform creates with the window, since the pixel format of such a window is chosen before it opens. The devices of those APIs draw through it.
class GlContext {
  public:
    GlContext() = default;
    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;
    virtual ~GlContext() = default;

    virtual void makeCurrent() = 0;
    virtual void swapBuffers() = 0;
    virtual void setSwapInterval(int value) = 0;
    [[nodiscard]] virtual void* getProcAddress(const char* name) = 0;
};

// The window a device draws into, as the platform hands it over: the native handles of the window that the graphics API makes its swapchain on, the size of the drawable in pixels and how the frames reach the screen.
struct Surface {
    enum class Kind : unsigned char {
        // A `CAMetalLayer` in `layer`.
        MetalLayer,
        // An `HWND` in `window` and the `HINSTANCE` of the process in `display`.
        Win32,
        // An X11 `Window` in `window` and its `Display` in `display`.
        Xlib,
        // A Wayland `wl_surface` in `window` and its `wl_display` in `display`.
        Wayland,
        // An `ANativeWindow` in `window`.
        Android,
        // The canvas of the page under the selector `canvas`.
        WebCanvas,
        // The context of OpenGL, OpenGL ES or WebGL2 in `gl`, which owns the drawable.
        Gl,
        // The native handles of the window of a port in `window` and `display`, which only the device of the port reads.
        Port,
    };

    Kind kind = Kind::MetalLayer;
    void* layer = nullptr;
    void* window = nullptr;
    void* display = nullptr;
    std::string_view canvas;
    GlContext* gl = nullptr;
    int width = 0;
    int height = 0;
    int sampleCount = 1;
    bool vsync = true;

    // Whether the window shows what lies behind it through the alpha of its pixels, which the swapchain then composites as premultiplied.
    bool transparent = false;
};

} // namespace haylen::gpu
