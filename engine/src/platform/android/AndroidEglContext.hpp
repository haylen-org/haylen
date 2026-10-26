#pragma once

#include <EGL/egl.h>
#include <android/native_window.h>

#include "haylen/gpu/Surface.hpp"

namespace haylen::platform {

// The EGL context of OpenGL ES that an Android activity draws through, which lasts while the activity lives. It asks for the newest version of OpenGL ES that the device declares, since an emulator creates a context of exactly the version an app asks for. Its window surface comes and goes with the window of the activity, and without one the context stays current on no surface, so the ticks of a paused app still create and release the objects of the GPU. It runs on the frame thread.
class AndroidEglContext final : public gpu::GlContext {
  public:
    // Opens the display and creates the context for windows of the sample count, with a depth and stencil buffer, and with debug output when `validation` asks for it. Throws `std::runtime_error` with a clear sentence when EGL cannot.
    AndroidEglContext(int sampleCount, bool validation);
    ~AndroidEglContext() override;

    void makeCurrent() override;
    void swapBuffers() override;
    void setSwapInterval(int value) override;
    [[nodiscard]] void* getProcAddress(const char* name) override;

    // Draws into the window from now on, which it may already do. Throws `std::runtime_error` when EGL cannot make a surface of it.
    void attachWindow(ANativeWindow& window);

    // Lets go of the window, whose objects the context keeps.
    void releaseWindow() noexcept;

    // Whether the GPU lost the context, such as after a reset of the driver, after which a new context replaces it.
    [[nodiscard]] bool isLost() const noexcept {
        return lost;
    }

  private:
    [[nodiscard]] EGLConfig chooseConfig(int sampleCount) const;
    [[nodiscard]] bool isExact(EGLConfig candidate, EGLint samples) const;
    [[nodiscard]] EGLint readAttribute(EGLConfig candidate, EGLint attribute) const;
    [[nodiscard]] bool hasExtension(const char* name) const;

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLConfig config = nullptr;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    ANativeWindow* attached = nullptr;
    bool lost = false;
};

} // namespace haylen::platform
