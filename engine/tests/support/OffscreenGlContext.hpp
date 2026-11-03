#pragma once

#include <memory>

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::test {

// An OpenGL 4.3 core, OpenGL ES 3.0 or WebGL2 context that draws into a hidden drawable, for the tests of the OpenGL device: a pixel buffer of EGL without a window, such as on Mesa in a container or on the Android emulator, a hidden window of WGL on Windows, or the canvas of the page on the web. Its drawable is the swapchain of the device.
class OffscreenGlContext final : public gpu::GlContext {
  public:
    // Throws `std::runtime_error` with the reason when the machine cannot make the context of the API.
    OffscreenGlContext(gpu::Device::Api api, int pixelsWide, int pixelsHigh);
    OffscreenGlContext(const OffscreenGlContext&) = delete;
    OffscreenGlContext& operator=(const OffscreenGlContext&) = delete;
    ~OffscreenGlContext() override;

    void makeCurrent() override;
    void swapBuffers() override;
    void setSwapInterval(int value) override;
    [[nodiscard]] void* getProcAddress(const char* name) override;

    // The surface that hands the context to the device.
    [[nodiscard]] gpu::Surface getSurface();

    // Another drawable that the context draws into, a pixel buffer of the same configuration of EGL or another hidden window of the same pixel format on Windows, which a test hands to the device as a surface beyond the window. Throws `std::runtime_error` where the context has no other drawable, as on the web.
    [[nodiscard]] std::unique_ptr<gpu::GlContext> makeDrawable(int pixelsWide, int pixelsHigh);

  private:
    class Native;
    class Drawable;

    void* display = nullptr;
    void* surface = nullptr;
    void* context = nullptr;
    void* config = nullptr;
    int width = 0;
    int height = 0;
};

} // namespace haylen::test
