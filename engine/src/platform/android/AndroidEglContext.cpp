#include "platform/android/AndroidEglContext.hpp"

#include <EGL/eglext.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "platform/android/AndroidGlesVersion.hpp"

namespace haylen::platform {

AndroidEglContext::AndroidEglContext(int sampleCount, bool validation) : display(eglGetDisplay(EGL_DEFAULT_DISPLAY)) {
    if (display == EGL_NO_DISPLAY || eglInitialize(display, nullptr, nullptr) == EGL_FALSE) {
        throw std::runtime_error("EGL found no display to draw OpenGL ES on.");
    }
    config = chooseConfig(sampleCount);
    if (config == nullptr) {
        eglTerminate(display);
        throw std::runtime_error("EGL offers no configuration of OpenGL ES 3 with 8 bits of red, green and blue and a depth and stencil buffer.");
    }

    // Devices that predate OpenGL ES 3 declare an older version, and an app still asks for 3.0 there.
    const AndroidGlesVersion declared = AndroidGlesVersion::read();
    const AndroidGlesVersion version = declared.major >= 3 ? declared : AndroidGlesVersion{};
    const std::vector<EGLint> attributes = describeContext(version, validation);
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attributes.data());
    if (context == EGL_NO_CONTEXT) {
        eglTerminate(display);
        throw std::runtime_error("EGL could not create a context of OpenGL ES " + std::to_string(version.major) + "." + std::to_string(version.minor) + ".");
    }
    makeCurrent();
}

AndroidEglContext::~AndroidEglContext() {
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
    }
    eglDestroyContext(display, context);
    eglTerminate(display);
}

void AndroidEglContext::makeCurrent() {
    if (eglMakeCurrent(display, surface, surface, context) == EGL_TRUE) {
        return;
    }
    lost = eglGetError() == EGL_CONTEXT_LOST;
    throw std::runtime_error("EGL could not make the context of OpenGL ES current.");
}

void AndroidEglContext::swapBuffers() {
    if (surface != EGL_NO_SURFACE && eglSwapBuffers(display, surface) == EGL_FALSE && eglGetError() == EGL_CONTEXT_LOST) {
        lost = true;
    }
}

void AndroidEglContext::setSwapInterval(int value) {
    eglSwapInterval(display, value);
}

void* AndroidEglContext::getProcAddress(const char* name) {
    return reinterpret_cast<void*>(eglGetProcAddress(name));
}

void AndroidEglContext::attachWindow(ANativeWindow& window) {
    if (attached == &window) {
        return;
    }
    releaseWindow();
    surface = eglCreateWindowSurface(display, config, &window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        throw std::runtime_error("EGL could not make a surface of the window of the activity.");
    }
    attached = &window;
    makeCurrent();
}

// The context stays current on no surface, which EGL_KHR_surfaceless_context of every device with OpenGL ES 3 allows.
void AndroidEglContext::releaseWindow() noexcept {
    if (surface == EGL_NO_SURFACE) {
        return;
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
    eglDestroySurface(display, surface);
    surface = EGL_NO_SURFACE;
    attached = nullptr;
}

// A robust context loses itself on a reset of the GPU, which the device of OpenGL ES then reports as lost, where EGL can make one.
std::vector<EGLint> AndroidEglContext::describeContext(const AndroidGlesVersion& version, bool validation) const {
    std::vector<EGLint> attributes{EGL_CONTEXT_MAJOR_VERSION_KHR, version.major, EGL_CONTEXT_MINOR_VERSION_KHR, version.minor};
    if (hasExtension("EGL_EXT_create_context_robustness")) {
        attributes.insert(attributes.end(), {EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_LOSE_CONTEXT_ON_RESET_EXT});
    }
    if (validation && hasExtension("EGL_KHR_create_context")) {
        attributes.insert(attributes.end(), {EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR});
    }
    attributes.push_back(EGL_NONE);
    return attributes;
}

// The first configuration that EGL offers may carry more bits than asked for, so the one with exactly the asked sizes wins.
EGLConfig AndroidEglContext::chooseConfig(int sampleCount) const {
    const EGLint samples = sampleCount > 1 ? sampleCount : 0;
    const std::array<EGLint, 21> attributes{
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_SAMPLE_BUFFERS, samples > 0 ? 1 : 0, EGL_SAMPLES, samples, EGL_NONE,
    };
    EGLint available = 0;
    if (eglChooseConfig(display, attributes.data(), nullptr, 0, &available) == EGL_FALSE || available <= 0) {
        return nullptr;
    }
    std::vector<EGLConfig> configs(static_cast<std::size_t>(available));
    eglChooseConfig(display, attributes.data(), configs.data(), available, &available);
    configs.resize(static_cast<std::size_t>(std::max(available, 0)));

    const auto found = std::ranges::find_if(configs, [this, samples](EGLConfig candidate) { return isExact(candidate, samples); });
    if (found != configs.end()) {
        return *found;
    }
    return configs.empty() ? nullptr : configs.front();
}

bool AndroidEglContext::isExact(EGLConfig candidate, EGLint samples) const {
    return readAttribute(candidate, EGL_RED_SIZE) == 8 && readAttribute(candidate, EGL_GREEN_SIZE) == 8 && readAttribute(candidate, EGL_BLUE_SIZE) == 8 && readAttribute(candidate, EGL_ALPHA_SIZE) == 0 && readAttribute(candidate, EGL_DEPTH_SIZE) == 24 && readAttribute(candidate, EGL_STENCIL_SIZE) == 8 && readAttribute(candidate, EGL_SAMPLES) == samples;
}

EGLint AndroidEglContext::readAttribute(EGLConfig candidate, EGLint attribute) const {
    EGLint value = -1;
    eglGetConfigAttrib(display, candidate, attribute, &value);
    return value;
}

bool AndroidEglContext::hasExtension(const char* name) const {
    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    if (extensions == nullptr) {
        return false;
    }
    const std::string_view list(extensions);
    const std::string_view wanted(name);
    for (std::size_t start = 0; start < list.size();) {
        const std::size_t end = std::min(list.find(' ', start), list.size());
        if (list.substr(start, end - start) == wanted) {
            return true;
        }
        start = end + 1;
    }
    return false;
}

} // namespace haylen::platform
