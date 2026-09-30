#pragma once

struct lua_State;

namespace haylen::navigation2d {

// Installs `haylen.navigation2d` with its grids, maps, flow fields, hierarchies, graphs, navigation meshes, steering agents and crowds.
class Navigation2DLua final {
  public:
    static void install(lua_State* L);

  private:
    static int open(lua_State* L);
};

} // namespace haylen::navigation2d
