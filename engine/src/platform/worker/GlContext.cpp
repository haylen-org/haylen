#include "platform/worker/GlContext.hpp"

#include <stdexcept>

namespace haylen::platform::worker {

// Emscripten finds the canvas of the module, the `OffscreenCanvas` of the worker, under `#canvas`. The framebuffer holds the depth and stencil that the swapchain passes draw with, an alpha channel only for a transparent window, and the samples of the window.
GlContext::GlContext(const core::AppConfig::Window& options) {
    EmscriptenWebGLContextAttributes attributes;
    emscripten_webgl_init_context_attributes(&attributes);
    attributes.majorVersion = 2;
    attributes.minorVersion = 0;
    attributes.alpha = options.transparent ? EM_TRUE : EM_FALSE;
    attributes.depth = EM_TRUE;
    attributes.stencil = EM_TRUE;
    attributes.antialias = options.sampleCount > 1 ? EM_TRUE : EM_FALSE;
    context = emscripten_webgl_create_context("#canvas", &attributes);
    if (context <= 0) {
        throw std::runtime_error("The canvas of the worker gave no WebGL2 context.");
    }

    makeCurrent();
}

GlContext::~GlContext() {
    emscripten_webgl_destroy_context(context);
}

void GlContext::makeCurrent() {
    emscripten_webgl_make_context_current(context);
}

// The browser shows what the worker drew once its animation frame returns, so a swap has nothing to do.
void GlContext::swapBuffers() {}

// The animation frames of the worker pace the frames, so the context has no interval of its own.
void GlContext::setSwapInterval(int) {}

void* GlContext::getProcAddress(const char* name) {
    return emscripten_webgl_get_proc_address(name);
}

} // namespace haylen::platform::worker
