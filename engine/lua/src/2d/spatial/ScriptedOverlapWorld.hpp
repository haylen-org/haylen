#pragma once

#include <vector>

#include "haylen/2d/spatial/OverlapWorld.hpp"

namespace haylen::lua::spatial2d {

// An overlap world in Lua with the buffer of contacts it fills, so a step reuses its memory.
struct ScriptedOverlapWorld {
    haylen::spatial2d::OverlapWorld world;
    std::vector<haylen::spatial2d::OverlapWorld::Contact> contacts{};
};

} // namespace haylen::lua::spatial2d
