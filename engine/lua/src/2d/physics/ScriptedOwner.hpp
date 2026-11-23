#pragma once

#include <memory>
#include <utility>

namespace haylen::physics2d {

class World;

// A terrain or fluid held by Lua. It keeps its world alive and goes before it, because it destroys the bodies it owns in that world. Its user value is the Lua world object, which reaches its bodies.
template <typename Owner> struct ScriptedOwner {
    template <typename... Args> explicit ScriptedOwner(std::shared_ptr<World> keeper, Args&&... args) : world(std::move(keeper)), object(*world, std::forward<Args>(args)...) {}

    std::shared_ptr<World> world;
    Owner object;
};

} // namespace haylen::physics2d
