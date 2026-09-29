#pragma once

#include <memory>

#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/2d/navigation/GridSearch.hpp"

namespace haylen::navigation2d {

// A navigation grid created from Lua, with the search that serves its paths so they reuse its buffers, and the copy that background work shares until the grid changes.
struct ScriptedGrid {
    Grid grid;
    GridSearch search;
    std::shared_ptr<const Grid> snapshot;
};

} // namespace haylen::navigation2d
