#include "support/OffscreenGlContext.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <array>
#include <format>
#include <stdexcept>

namespace haylen::test {

// Mesa without a display server draws through its surfaceless platform, and Android has one display.
OffscreenGlContext::OffscreenGlContext(gpu::Device::Api api, int pixelsWide, int pixelsHigh) : width(pixelsWide), height(pixelsHigh) {
#if defined(__ANDROID__)
    EGLDisplay made = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#else
    EGLDisplay made = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
#endif
    if (made == EGL_NO_DISPLAY || eglInitialize(made, nullptr, nullptr) == EGL_FALSE) {
        throw std::runtime_error("EGL has no display on this machine.");
    }
    display = made;

    const bool desktop = api == gpu::Device::Api::OpenGl;
    if (eglBindAPI(desktop ? EGL_OPENGL_API : EGL_OPENGL_ES_API) == EGL_FALSE) {
        throw std::runtime_error(std::format("EGL does not run {} on this machine.", desktop ? "OpenGL" : "OpenGL ES"));
    }
    const std::array<EGLint, 17> configAttributes{EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, desktop ? EGL_OPENGL_BIT : EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
    EGLConfig config = nullptr;
    EGLint configs = 0;
    if (eglChooseConfig(made, configAttributes.data(), &config, 1, &configs) == EGL_FALSE || configs == 0) {
        throw std::runtime_error("EGL has no configuration with a pixel buffer for the context.");
    }

    const std::array<EGLint, 5> surfaceAttributes{EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
    surface = eglCreatePbufferSurface(made, config, surfaceAttributes.data());
    if (surface == EGL_NO_SURFACE) {
        throw std::runtime_error("EGL could not make the pixel buffer of the context.");
    }

    const std::array<EGLint, 9> desktopAttributes{EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE, EGL_NONE};
    const std::array<EGLint, 7> embeddedAttributes{EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE, EGL_NONE};
    context = eglCreateContext(made, config, EGL_NO_CONTEXT, desktop ? desktopAttributes.data() : embeddedAttributes.data());
    if (context == EGL_NO_CONTEXT) {
        eglDestroySurface(made, surface);
        throw std::runtime_error(std::format("EGL could not make an {} context.", desktop ? "OpenGL 4.3 core" : "OpenGL ES 3.0"));
    }
    makeCurrent();
}

OffscreenGlContext::~OffscreenGlContext() {
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
}

void OffscreenGlContext::makeCurrent() {
    eglMakeCurrent(display, surface, surface, context);
}

void OffscreenGlContext::swapBuffers() {
    eglSwapBuffers(display, surface);
}

void OffscreenGlContext::setSwapInterval(int value) {
    eglSwapInterval(display, value);
}

void* OffscreenGlContext::getProcAddress(const char* name) {
    return reinterpret_cast<void*>(eglGetProcAddress(name));
}

gpu::Surface OffscreenGlContext::getSurface() {
    return {.kind = gpu::Surface::Kind::Gl, .gl = this, .width = width, .height = height};
}

} // namespace haylen::test
