#pragma once

#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// One corner of a mesh triangle, with texture coordinates from 0 to 1.
struct MeshVertex {
    math::Vec2 position{};
    math::Vec2 uv{};
    math::Color color = math::Color::white();
};

} // namespace haylen::graphics2d
