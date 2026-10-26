#include "gpu/support/OffscreenSurface.hpp"

#include <stdexcept>

#include "gpu/DeviceFactory.hpp"

#if defined(HAYLEN_GPU_OPENGL) || defined(HAYLEN_GPU_OPENGLES)
#include "support/OffscreenGlContext.hpp"
#endif

namespace haylen::test {

OffscreenSurface::OffscreenSurface(gpu::Device::Api api) {
    if (!gpu::DeviceFactory::usesGlContext(api)) {
        return;
    }
#if defined(HAYLEN_GPU_OPENGL) || defined(HAYLEN_GPU_OPENGLES)
    if (api != gpu::Device::Api::WebGl2) {
        auto made = std::make_unique<OffscreenGlContext>(api, kWidth, kHeight);
        surface = made->getSurface();
        context = std::move(made);
        return;
    }
#endif
    throw std::runtime_error("The GPU tests make no context of this API without a window, since only the page of a browser holds WebGL2 and only the OpenGL device brings contexts of OpenGL.");
}

OffscreenSurface::~OffscreenSurface() = default;

} // namespace haylen::test
