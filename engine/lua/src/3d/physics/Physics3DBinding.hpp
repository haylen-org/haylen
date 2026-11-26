#pragma once

#include <lua.hpp>

#include <array>
#include <memory>
#include <string_view>

#include "haylen/3d/physics/World.hpp"
#include "haylen/lua/Type.hpp"

namespace haylen::lua {

template <> struct Type<haylen::physics3d::World> {
    static constexpr const char* name = "haylen.PhysicsWorld3D";
    using Storage = std::shared_ptr<haylen::physics3d::World>;
};

} // namespace haylen::lua

namespace haylen::lua::physics3d {

// Installs `haylen.physics3d` with the `PhysicsWorld3D` class, the worlds in meters that the selected backend simulates.
class Physics3DBinding final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kOwnerField{"owner"};

    static int newWorld(lua_State* L);
    static int getBackends(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::physics3d
