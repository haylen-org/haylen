#include "gpu/support/SecondSurface.hpp"

#import <QuartzCore/CAMetalLayer.h>

#include <stdexcept>

namespace haylen::test {

// A `CAMetalLayer` outside every window, whose drawables the Metal device draws into as into those of a window.
class SecondSurface::Native final {
  public:
    Native(int width, int height) : layer([CAMetalLayer layer]) {
        layer.drawableSize = CGSizeMake(width, height);
    }

    [[nodiscard]] gpu::Surface describe() const noexcept {
        return {.kind = gpu::Surface::Kind::MetalLayer, .layer = (__bridge void*)layer};
    }

  private:
    CAMetalLayer* layer;
};

SecondSurface::SecondSurface(gpu::Device::Api api, OffscreenSurface&, int width, int height) {
    if (api != gpu::Device::Api::Metal) {
        throw std::runtime_error("Apple platforms draw only through Metal.");
    }
    native = std::make_unique<Native>(width, height);
    surface = native->describe();
    surface.width = width;
    surface.height = height;
}

SecondSurface::~SecondSurface() = default;

} // namespace haylen::test
