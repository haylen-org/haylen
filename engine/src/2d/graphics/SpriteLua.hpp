#pragma once

struct lua_State;

namespace haylen::graphics2d {

// Installs the `Sprite` class of `haylen.graphics2d`, whose properties write straight into the sprite that `draw` submits.
class SpriteLua final {
  public:
    static void install(lua_State* L);

  private:
    static int getMaterial(lua_State* L);
    static int setMaterial(lua_State* L);
    static int getPartMask(lua_State* L);
    static int setPartMask(lua_State* L);
    static int getNormalMap(lua_State* L);
    static int setNormalMap(lua_State* L);
    static int draw(lua_State* L);
};

} // namespace haylen::graphics2d
