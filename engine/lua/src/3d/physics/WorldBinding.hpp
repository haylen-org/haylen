#pragma once

#include <lua.hpp>

#include <type_traits>

#include "3d/physics/Physics3DBinding.hpp"
#include "haylen/3d/physics/World.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::physics3d {

// Installs the `PhysicsWorld3D` class of `haylen.physics3d`.
class WorldBinding final {
  public:
    static void install(lua_State* L);

  private:
    // Returns the live world of the first argument, and raises an error once it was destroyed.
    [[nodiscard]] static haylen::physics3d::World& check(lua_State* L);

    // A property that reads a getter of the world.
    template <auto Getter> static int get(lua_State* L) {
        lua::Stack::push(L, (check(L).*Getter)());
        return 1;
    }

    // A property that writes a setter of the world with the value after the key.
    template <auto Setter, typename Value> static int set(lua_State* L) {
        (check(L).*Setter)(lua::Stack::read<Value>(L, 3));
        return 0;
    }

    // A property that reads a limit the world was made with.
    template <auto Field> static int getLimit(lua_State* L) {
        lua::Stack::push(L, check(L).getSettings().*Field);
        return 1;
    }

    static int step(lua_State* L);
    static int destroy(lua_State* L);
    static int isDestroyed(lua_State* L);
    static int stats(lua_State* L);
    static int stateHash(lua_State* L);
    static int getBackend(lua_State* L);
    static int getBodyCount(lua_State* L);
    static int getActiveBodyCount(lua_State* L);
};

} // namespace haylen::lua::physics3d
