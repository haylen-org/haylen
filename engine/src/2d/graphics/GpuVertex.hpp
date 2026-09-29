#pragma once

#include <cstdint>

namespace haylen::graphics2d {

// One mesh vertex as the mesh shader reads it from the vertex buffer.
struct GpuVertex {
    float position[2];
    float uv[2];
    std::uint32_t color;
};

} // namespace haylen::graphics2d
