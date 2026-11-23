#pragma once

struct lua_State;

namespace haylen::lua::spatial2d {

// Installs `haylen.spatial2d` with its spatial structures, its grid algorithms and screen picking.
class Spatial2DBinding final {
  public:
    static void install(lua_State* L);

  private:
    static int screenRay(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::spatial2d
