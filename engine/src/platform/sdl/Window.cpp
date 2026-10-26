#include "platform/sdl/Window.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace haylen::platform::sdl {

Window::~Window() {
    close();
}

void Window::open(const core::AppConfig::Window& options, gpu::Device::Api value) {
    close();
    api = value;
    sampleCount = options.sampleCount;
    vsync = options.vsync;
    transparent = options.transparent;
    describeGl(options, api);

    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, options.title.c_str());
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, options.width);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, options.height);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, static_cast<std::int64_t>(getFlags(options, api)));
    window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    if (window == nullptr) {
        throw std::runtime_error("The window could not open. " + std::string(SDL_GetError()));
    }

    // Desktops whose window coordinates are pixels, as on Windows and X11, open the window as large in points as on the others.
    const float points = SDL_GetWindowDisplayScale(window) / SDL_GetWindowPixelDensity(window);
    if (options.highDpi && points != 1.0F) {
        SDL_SetWindowSize(window, static_cast<int>(std::lround(static_cast<float>(options.width) * points)), static_cast<int>(std::lround(static_cast<float>(options.height) * points)));
    }

    try {
        attachApi();
    } catch (...) {
        close();
        throw;
    }
    readHandles();
    refresh();
    opened.store(true);
}

void Window::close() noexcept {
    if (window == nullptr) {
        return;
    }
    opened.store(false);
    handle.store(nullptr);
    display.store(nullptr);
    gl.reset();
    if (metalView != nullptr) {
        SDL_Metal_DestroyView(metalView);
        metalView = nullptr;
    }
    SDL_DestroyWindow(window);
    window = nullptr;
}

void Window::show() {
    SDL_ShowWindow(window);
}

gpu::Surface Window::getSurface() const {
    gpu::Surface surface = getNativeSurface();
    SDL_GetWindowSizeInPixels(window, &surface.width, &surface.height);
    surface.sampleCount = sampleCount;
    surface.vsync = vsync;
    surface.transparent = transparent;
    return surface;
}

void Window::refresh() noexcept {
    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
    width.store(static_cast<float>(pixelWidth));
    height.store(static_cast<float>(pixelHeight));
    scale.store(SDL_GetWindowPixelDensity(window));
}

math::Vec2 Window::getPixelSize() const noexcept {
    return {width.load(), height.load()};
}

float Window::getPixelDensity() const noexcept {
    return scale.load();
}

bool Window::queryOpen() const noexcept {
    return opened.load();
}

void* Window::queryHandle() const noexcept {
    return handle.load();
}

void* Window::queryDisplay() const noexcept {
    return display.load();
}

void* Window::queryActivity() const noexcept {
    return nullptr;
}

math::Vec2 Window::queryFramebufferSize() const noexcept {
    return getPixelSize();
}

float Window::queryScale() const noexcept {
    return scale.load();
}

// A desktop window opens hidden, so it first shows with the style that the app gives it as it starts. A window of UIKit shows at once under the launch screen, since a hidden one misses the turn of its scene to the orientation of the app.
SDL_WindowFlags Window::getFlags(const core::AppConfig::Window& options, gpu::Device::Api api) noexcept {
    SDL_WindowFlags flags = kHidden ? SDL_WINDOW_HIDDEN : 0;
    if (options.highDpi) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    if (options.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (options.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (!options.decorated) {
        flags |= SDL_WINDOW_BORDERLESS;
    }
    if (options.transparent) {
        flags |= SDL_WINDOW_TRANSPARENT;
    }
    if (options.alwaysOnTop) {
        flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    }
    if (!options.focusable) {
        flags |= SDL_WINDOW_NOT_FOCUSABLE;
    }

    switch (api) {
    case gpu::Device::Api::Metal:
        return flags | SDL_WINDOW_METAL;
    case gpu::Device::Api::Vulkan:
        return flags | SDL_WINDOW_VULKAN;
    case gpu::Device::Api::OpenGl:
    case gpu::Device::Api::OpenGlEs:
    case gpu::Device::Api::WebGl2:
        return flags | SDL_WINDOW_OPENGL;
    default:
        return flags;
    }
}

// The pixel format of a window of OpenGL is chosen before it opens: the version of the context, the samples of the window and the depth, stencil and alpha of its framebuffer, which the swapchain passes of the device draw into.
void Window::describeGl(const core::AppConfig::Window& options, gpu::Device::Api api) {
    SDL_GL_ResetAttributes();
    if (api == gpu::Device::Api::OpenGl) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    } else if (api == gpu::Device::Api::OpenGlEs || api == gpu::Device::Api::WebGl2) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    } else {
        return;
    }
#if !defined(NDEBUG)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, options.transparent ? 8 : 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, options.sampleCount > 1 ? 1 : 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, options.sampleCount > 1 ? options.sampleCount : 0);
}

void Window::attachApi() {
    switch (api) {
    case gpu::Device::Api::Metal:
        metalView = SDL_Metal_CreateView(window);
        if (metalView == nullptr) {
            throw std::runtime_error("The window could not create its Metal layer. " + std::string(SDL_GetError()));
        }
        break;
    case gpu::Device::Api::OpenGl:
    case gpu::Device::Api::OpenGlEs:
    case gpu::Device::Api::WebGl2:
        gl = std::make_unique<GlContext>(window);
        break;
    default:
        break;
    }
}

// Services read the window of the system: an `NSWindow` on macOS, a `UIWindow` on the other Apple platforms, an `HWND` on Windows and an X11 window with its display on Linux. A Wayland window and the canvas of the web have no handle that they read.
void Window::readHandles() noexcept {
    [[maybe_unused]] const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
#if defined(__APPLE__) && TARGET_OS_OSX
    handle.store(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr));
#elif defined(__APPLE__)
    handle.store(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr));
#elif defined(_WIN32)
    handle.store(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
#elif defined(__linux__)
    if (std::string_view(SDL_GetCurrentVideoDriver()) == "x11") {
        handle.store(reinterpret_cast<void*>(static_cast<std::uintptr_t>(SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0))));
        display.store(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
    }
#endif
}

gpu::Surface Window::getNativeSurface() const {
    if (gl != nullptr) {
        return {.kind = gpu::Surface::Kind::Gl, .gl = gl.get()};
    }
    if (metalView != nullptr) {
        return {.kind = gpu::Surface::Kind::MetalLayer, .layer = SDL_Metal_GetLayer(metalView)};
    }

#if defined(_WIN32)
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    return {.kind = gpu::Surface::Kind::Win32, .window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr), .display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr)};
#elif defined(__EMSCRIPTEN__)
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    return {.kind = gpu::Surface::Kind::WebCanvas, .canvas = SDL_GetStringProperty(properties, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING, "")};
#elif defined(__linux__)
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    if (std::string_view(SDL_GetCurrentVideoDriver()) == "wayland") {
        return {.kind = gpu::Surface::Kind::Wayland, .window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr), .display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr)};
    }
    return {.kind = gpu::Surface::Kind::Xlib, .window = reinterpret_cast<void*>(static_cast<std::uintptr_t>(SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0))), .display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr)};
#else
    throw std::runtime_error("The window has no surface for the graphics API.");
#endif
}

} // namespace haylen::platform::sdl
