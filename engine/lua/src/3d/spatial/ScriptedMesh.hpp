#pragma once

#include <cstdint>
#include <vector>

#include "haylen/3d/spatial/MeshBvh.hpp"

namespace haylen::lua::spatial3d {

// A mesh hierarchy created from Lua, with the list of triangles that its queries refill, so queries that repeat allocate nothing.
struct ScriptedMesh {
    haylen::spatial3d::MeshBvh mesh;
    std::vector<std::uint32_t> triangles{};
};

} // namespace haylen::lua::spatial3d
