#pragma once

struct lua_State;

namespace haylen::lua::navigation3d {

// Installs `haylen.navigation3d` with its geometry, navigation meshes, query filters and crowds.
class Navigation3DBinding final {
  public:
    static void install(lua_State* L);

  private:
    static int open(lua_State* L);
};

} // namespace haylen::lua::navigation3d
