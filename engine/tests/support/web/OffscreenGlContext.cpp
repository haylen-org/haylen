#include "support/OffscreenGlContext.hpp"

#include <emscripten/html5_webgl.h>

#include <cstdint>
#include <stdexcept>

namespace haylen::test {

// The page holds WebGL2 on its canvas, which draws hidden in a browser without a window, and the context holds the handle that Emscripten gives the WebGL2 context of the canvas.
OffscreenGlContext::OffscreenGlContext(gpu::Device::Api api, int pixelsWide, int pixelsHigh) : width(pixelsWide), height(pixelsHigh) {
    if (api != gpu::Device::Api::WebGl2) {
        throw std::runtime_error("The page of a browser runs WebGL2 alone, without OpenGL or OpenGL ES.");
    }
    EmscriptenWebGLContextAttributes attributes;
    emscripten_webgl_init_context_attributes(&attributes);
    attributes.majorVersion = 2;
    attributes.minorVersion = 0;
    attributes.depth = EM_TRUE;
    attributes.stencil = EM_TRUE;
    attributes.antialias = EM_FALSE;
    const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle = emscripten_webgl_create_context("#canvas", &attributes);
    if (handle <= 0) {
        throw std::runtime_error("The page has no canvas with a WebGL2 context.");
    }
    context = reinterpret_cast<void*>(static_cast<std::intptr_t>(handle));
    makeCurrent();
}

OffscreenGlContext::~OffscreenGlContext() {
    emscripten_webgl_destroy_context(static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(reinterpret_cast<std::intptr_t>(context)));
}

void OffscreenGlContext::makeCurrent() {
    emscripten_webgl_make_context_current(static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(reinterpret_cast<std::intptr_t>(context)));
}

// The browser shows what the page drew when the page yields, so a swap has nothing to do.
void OffscreenGlContext::swapBuffers() {}

void OffscreenGlContext::setSwapInterval(int) {}

void* OffscreenGlContext::getProcAddress(const char* name) {
    return emscripten_webgl_get_proc_address(name);
}

gpu::Surface OffscreenGlContext::getSurface() {
    return {.kind = gpu::Surface::Kind::Gl, .gl = this, .width = width, .height = height};
}

} // namespace haylen::test
