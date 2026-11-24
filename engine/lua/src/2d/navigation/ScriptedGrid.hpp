#pragma once

#include <memory>

#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/2d/navigation/GridSearch.hpp"

namespace haylen::lua::navigation2d {

// A navigation grid created from Lua, with the search that serves its paths so they reuse its buffers, and the copy that background work shares until the grid changes.
struct ScriptedGrid {
    haylen::navigation2d::Grid grid;
    haylen::navigation2d::GridSearch search;
    std::shared_ptr<const haylen::navigation2d::Grid> snapshot{};
};

} // namespace haylen::lua::navigation2d
