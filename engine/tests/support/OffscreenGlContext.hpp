#pragma once

#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::test {

// An OpenGL 4.3 core or OpenGL ES 3.0 context on a hidden pixel buffer, made through EGL without a window, such as on Mesa in a container or on the Android emulator, for the tests of the OpenGL device. Its drawable is the swapchain of the device.
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

  private:
    void* display = nullptr;
    void* surface = nullptr;
    void* context = nullptr;
    int width = 0;
    int height = 0;
};

} // namespace haylen::test
