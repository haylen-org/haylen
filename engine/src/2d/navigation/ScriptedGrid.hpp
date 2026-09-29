#pragma once

#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/2d/navigation/GridSearch.hpp"

namespace haylen::navigation2d {

// A navigation grid created from Lua, with the search that serves its paths so they reuse its buffers.
struct ScriptedGrid {
    Grid grid;
    GridSearch search;
};

} // namespace haylen::navigation2d
