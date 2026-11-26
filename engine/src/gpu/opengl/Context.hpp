#pragma once

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/StateCache.hpp"

namespace haylen::gpu::opengl {

// The entry points of the context of a device, the state the device set in it and whether the device found the context lost, which the readbacks of the device share, so a readback ends once the device is lost and touches nothing once its device is gone.
struct Context {
    Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Gl gl;
    StateCache state{gl};
    bool lost = false;
};

} // namespace haylen::gpu::opengl
