#pragma once

struct lua_State;

namespace haylen::graphics2d {

// Installs the Sprite class of haylen.graphics2d, whose properties write straight into the sprite that draw submits.
class SpriteLua final {
  public:
    static void install(lua_State* L);

  private:
    static int getFlipX(lua_State* L);
    static int setFlipX(lua_State* L);
    static int getFlipY(lua_State* L);
    static int setFlipY(lua_State* L);
    static int getFlipDiagonal(lua_State* L);
    static int setFlipDiagonal(lua_State* L);
    static int getMaterial(lua_State* L);
    static int setMaterial(lua_State* L);
    static int getNormalMap(lua_State* L);
    static int setNormalMap(lua_State* L);
    static int draw(lua_State* L);
};

} // namespace haylen::graphics2d
