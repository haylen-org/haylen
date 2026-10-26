#include "gpu/support/OffscreenSurface.hpp"

#include <stdexcept>

#include "gpu/DeviceFactory.hpp"

#if defined(HAYLEN_GPU_OPENGL) || defined(HAYLEN_GPU_OPENGLES) || defined(HAYLEN_GPU_WEBGL2)
#include "support/OffscreenGlContext.hpp"
#endif

namespace haylen::test {

OffscreenSurface::OffscreenSurface(gpu::Device::Api api) {
    if (!gpu::DeviceFactory::usesGlContext(api)) {
        return;
    }
#if defined(HAYLEN_GPU_OPENGL) || defined(HAYLEN_GPU_OPENGLES) || defined(HAYLEN_GPU_WEBGL2)
    auto made = std::make_unique<OffscreenGlContext>(api, kWidth, kHeight);
    surface = made->getSurface();
    context = std::move(made);
#else
    throw std::runtime_error("The GPU tests make contexts of OpenGL only in builds with the OpenGL device, which brings them.");
#endif
}

OffscreenSurface::~OffscreenSurface() = default;

} // namespace haylen::test
