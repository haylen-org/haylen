#pragma once

#include "haylen/2d/navigation/Graph.hpp"
#include "haylen/2d/navigation/GraphSearch.hpp"

namespace haylen::navigation2d {

// A waypoint graph created from Lua, with the search that serves its paths so they reuse its buffers.
struct ScriptedGraph {
    Graph graph;
    GraphSearch search;
};

} // namespace haylen::navigation2d
