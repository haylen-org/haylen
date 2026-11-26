#pragma once

#include <cstdint>
#include <vector>

#include "haylen/3d/spatial/Neighbor.hpp"
#include "haylen/3d/spatial/RayHit.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Sphere.hpp"

namespace haylen::lua::spatial3d {

// A 3D spatial structure created from Lua. Lua keys entries by any value, usually the entity table itself, and only the binding knows the ids the structure sees. The lists keep their room between calls, so queries and batches that repeat allocate nothing.
template <typename Structure> struct ScriptedIndex {
    Structure index;
    std::uint64_t nextId = 1;
    std::vector<std::uint64_t> ids{};
    std::vector<haylen::math::Box> boxes{};
    std::vector<haylen::math::Sphere> spheres{};
    std::vector<haylen::spatial3d::RayHit> hits{};
    std::vector<haylen::spatial3d::Neighbor> neighbors{};
};

} // namespace haylen::lua::spatial3d
