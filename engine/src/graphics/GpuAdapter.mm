#include "graphics/GpuAdapter.hpp"

#import <Metal/Metal.h>

#include "sokol_gfx.h"

namespace haylen::graphics {

std::string GpuAdapter::getName() {
    const void* device = sg_mtl_device();
    if (device == nullptr) {
        return {};
    }
    const id<MTLDevice> metal = (__bridge id<MTLDevice>)device;
    return metal.name.UTF8String;
}

} // namespace haylen::graphics
