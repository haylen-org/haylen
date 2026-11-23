#pragma once

#include "haylen/2d/navigation/Graph.hpp"
#include "haylen/2d/navigation/GraphSearch.hpp"

namespace haylen::lua::navigation2d {

// A waypoint graph created from Lua, with the search that serves its paths so they reuse its buffers.
struct ScriptedGraph {
    haylen::navigation2d::Graph graph;
    haylen::navigation2d::GraphSearch search;
};

} // namespace haylen::lua::navigation2d
