#include "graphics/ShaderPrecompiler.hpp"

#import <Metal/Metal.h>

#include <utility>

#include "haylen/core/JobSystem.hpp"
#include "sokol_gfx.h"

namespace haylen::graphics {

// Each source compiles exactly as the Metal backend of sokol compiles it, so the program finds it in the cache. A source that does not compile fails again when its program is made, which reports the error.
void ShaderPrecompiler::start(core::JobSystem& jobs, std::vector<std::string> sources) {
    const void* device = sg_mtl_device();
    if (device == nullptr) {
        return;
    }
    const id<MTLDevice> metal = (__bridge id<MTLDevice>)device;
    for (std::string& source : sources) {
        // clang-format off
        jobs.postIo([metal, source = std::move(source)] {
            @autoreleasepool {
                (void)[metal newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()] options:nil error:nil];
            }
        });
        // clang-format on
    }
}

} // namespace haylen::graphics
