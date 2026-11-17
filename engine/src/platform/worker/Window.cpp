#include "platform/worker/Window.hpp"

#include <emscripten/em_macros.h>
#include <emscripten/emscripten.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "haylen/platform/PixelDensityCap.hpp"
#include "platform/web/WebPage.hpp"

// clang-format off
EM_JS_DEPS(haylen_worker_window, "$specialHTMLTargets");

// A worker has no document whose selector finds the canvas, so the canvas of the module answers to the selector "#canvas" through the targets of Emscripten.
EM_JS(void, haylen_js_name_canvas, (), {
    specialHTMLTargets["#canvas"] = Module.canvas;
});

EM_JS(void, haylen_js_canvas_box, (float* box), {
    const values = Module.haylen.canvasBox();
    for (let index = 0; index < 3; ++index) {
        HEAPF32[(box >> 2) + index] = values[index];
    }
});

EM_JS(void, haylen_js_size_canvas, (int width, int height), {
    if (Module.canvas.width !== width || Module.canvas.height !== height) {
        Module.canvas.width = width;
        Module.canvas.height = height;
    }
});
// clang-format on

namespace haylen::platform::worker {

Window::~Window() {
    close();
}

void Window::open(const core::AppConfig::Window& value, gpu::Device::Api graphicsApi) {
    close();
    options = value;
    api = graphicsApi;
    sampleCount = value.sampleCount;
    vsync = value.vsync;
    transparent = value.transparent;
    haylen_js_name_canvas();

    if (api == gpu::Device::Api::WebGl2) {
        gl = std::make_unique<GlContext>(options);
    }

    refresh();
    opened = true;
}

void Window::close() noexcept {
    opened = false;
    gl.reset();
    api = gpu::Device::Api::None;
}

// A WebGPU device draws into the canvas through the selector of the canvas of the module, and a WebGL2 device through the context of the canvas.
gpu::Surface Window::getSurface() const {
    gpu::Surface surface;
    if (gl != nullptr) {
        surface.kind = gpu::Surface::Kind::Gl;
        surface.gl = gl.get();
    } else {
        surface.kind = gpu::Surface::Kind::WebCanvas;
        surface.canvas = "#canvas";
    }

    surface.size = size;
    surface.sampleCount = sampleCount;
    surface.vsync = vsync;
    surface.transparent = transparent;
    return surface;
}

void Window::renewGlContext() {
    if (gl != nullptr) {
        gl.reset();
        gl = std::make_unique<GlContext>(options);
    }
}

// The canvas takes every pixel of the screen under its box, as the canvas of a page does, and the drawable shrinks from there to the cap, with the scale after it.
void Window::refresh() {
    std::array<float, 3> box{};
    haylen_js_canvas_box(box.data());
    const float density = box[2] > 0.0F ? box[2] : 1.0F;
    const int pixelWidth = std::max(1, static_cast<int>(std::lround(box[0] * density)));
    const int pixelHeight = std::max(1, static_cast<int>(std::lround(box[1] * density)));
    size = PixelDensityCap::apply(pixelWidth, pixelHeight, density, maxPixelDensity);
    scale = density * PixelDensityCap::getShare(size);

    if (gl != nullptr) {
        haylen_js_size_canvas(size.windowWidth, size.windowHeight);
    }

    WebPage::setDrawableWidth(size.width);
}

math::Vec2 Window::getPixelSize() const noexcept {
    return {static_cast<float>(size.width), static_cast<float>(size.height)};
}

bool Window::queryOpen() const noexcept {
    return opened;
}

void* Window::queryHandle() const noexcept {
    return nullptr;
}

void* Window::queryDisplay() const noexcept {
    return nullptr;
}

void* Window::queryActivity() const noexcept {
    return nullptr;
}

math::Vec2 Window::queryFramebufferSize() const noexcept {
    return getPixelSize();
}

float Window::queryScale() const noexcept {
    return scale;
}

} // namespace haylen::platform::worker
