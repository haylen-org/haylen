#pragma once

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/StateCache.hpp"

namespace haylen::gpu::opengl {

// The entry points of the context of a device and the state the device set in it, which the readbacks of the device share, so a readback that outlives its device finds no context and touches nothing.
struct Context {
    Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Gl gl;
    StateCache state{gl};
};

} // namespace haylen::gpu::opengl
