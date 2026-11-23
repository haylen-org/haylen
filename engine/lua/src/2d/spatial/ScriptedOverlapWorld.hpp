#pragma once

#include <vector>

#include "haylen/2d/spatial/OverlapWorld.hpp"

namespace haylen::spatial2d {

// An overlap world in Lua with the buffer of contacts it fills, so a step reuses its memory.
struct ScriptedOverlapWorld {
    OverlapWorld world;
    std::vector<OverlapWorld::Contact> contacts;
};

} // namespace haylen::spatial2d
