#pragma once

struct lua_State;

namespace haylen::graphics2d {

// Installs the Parallax class of haylen.graphics2d. Its drawing measures the view with the visible design area as the screen, as world canvases do.
class ParallaxLua final {
  public:
    static void install(lua_State* L);

  private:
    static int offset(lua_State* L);
    static int draw(lua_State* L);
};

} // namespace haylen::graphics2d
