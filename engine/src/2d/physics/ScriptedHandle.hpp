#pragma once

#include <memory>

namespace haylen::physics2d {

class World;

// A body, shape or joint handle held by Lua. It keeps its world alive, and its user value is the Lua world object, where per-body data and event callbacks live.
template <typename Handle> struct ScriptedHandle {
    std::shared_ptr<World> world;
    Handle handle;
};

} // namespace haylen::physics2d
