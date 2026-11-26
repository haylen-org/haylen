#pragma once

struct lua_State;

namespace haylen::lua::animation3d {

// Installs `haylen.animation3d` with its tracks, clips and clip players.
class Animation3DBinding final {
  public:
    static void install(lua_State* L);

  private:
    static int open(lua_State* L);
};

} // namespace haylen::lua::animation3d
