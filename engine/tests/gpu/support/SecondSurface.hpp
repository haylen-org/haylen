#pragma once

#include <memory>

#include "gpu/support/OffscreenSurface.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::test {

// A surface beyond the window for the GPU tests, which a device draws into after `addSurface`: another drawable of the offscreen context for the APIs that draw through a `gpu::GlContext`, a `CAMetalLayer` on Apple platforms, a hidden window of Windows, or a window of X11 on Linux while a display server runs. It never shows on the screen.
class SecondSurface final {
  public:
    // Throws `std::runtime_error` with the reason when this machine gives the API no such surface.
    SecondSurface(gpu::Device::Api api, OffscreenSurface& offscreen, int width, int height);
    SecondSurface(const SecondSurface&) = delete;
    SecondSurface& operator=(const SecondSurface&) = delete;
    ~SecondSurface();

    [[nodiscard]] const gpu::Surface& getSurface() const noexcept {
        return surface;
    }

  private:
    class Native;

    std::unique_ptr<gpu::GlContext> drawable;
    std::unique_ptr<Native> native;
    gpu::Surface surface;
};

} // namespace haylen::test
