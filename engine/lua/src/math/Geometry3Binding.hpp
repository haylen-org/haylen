#pragma once

#include <lua.hpp>

#include "haylen/math/Plane.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::lua::math {

// Installs the 3D shapes of `haylen.math`, starting with `Plane`.
class Geometry3Binding final {
  public:
    static void install(lua_State* L);

    // Sets `plane`, `planeFromPoint` and `planeFromPoints` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static int planeToString(lua_State* L);
    static int planeEqual(lua_State* L);
    static void checkNormal(haylen::math::Vec3 normal);
};

} // namespace haylen::lua::math
