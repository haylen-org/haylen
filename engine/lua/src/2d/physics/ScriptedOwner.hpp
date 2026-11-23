#pragma once

#include <memory>
#include <utility>

namespace haylen::physics2d {
class World;
}

namespace haylen::lua::physics2d {

// A terrain or fluid held by Lua. It keeps its world alive and goes before it, because it destroys the bodies it owns in that world. Its user value is the Lua world object, which reaches its bodies.
template <typename Owner> struct ScriptedOwner {
    template <typename... Args> explicit ScriptedOwner(std::shared_ptr<haylen::physics2d::World> keeper, Args&&... args) : world(std::move(keeper)), object(*world, std::forward<Args>(args)...) {}

    std::shared_ptr<haylen::physics2d::World> world;
    Owner object;
};

} // namespace haylen::lua::physics2d
