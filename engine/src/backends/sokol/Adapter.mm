#include "backends/sokol/Adapter.hpp"

#import <Metal/Metal.h>

#include "sokol_gfx.h"

namespace haylen::sokol {

std::string Adapter::getName() {
    const void* device = sg_mtl_device();
    if (device == nullptr) {
        return {};
    }
    const id<MTLDevice> metal = (__bridge id<MTLDevice>)device;
    return metal.name.UTF8String;
}

} // namespace haylen::sokol
