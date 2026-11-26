#pragma once

struct lua_State;

namespace haylen::graphics2d {
class Renderer;
struct Sprite;
} // namespace haylen::graphics2d

namespace haylen::lua::graphics2d {

// Installs the `Sprite` class of `haylen.graphics2d`, whose properties write straight into the sprite that `draw` submits.
class SpriteBinding final {
  public:
    static void install(lua_State* L);

  private:
    // Pushes a material or a texture of the sprite, or nil for an empty one, and reads one back, where nil empties it.
    template <typename Handle> static int pushHandle(lua_State* L, const Handle& handle);
    template <typename Handle> [[nodiscard]] static Handle readHandle(lua_State* L, int index);

    static int getMaterial(lua_State* L);
    static int setMaterial(lua_State* L);
    static int getPartMask(lua_State* L);
    static int setPartMask(lua_State* L);
    static int getNormalMap(lua_State* L);
    static int setNormalMap(lua_State* L);
    static int draw(lua_State* L);

    // Offers the sprite at index 1, which the renderer just drew, to the picking of the inspector, tagged by its address.
    static void offerPick(lua_State* L, haylen::graphics2d::Renderer& renderer, const haylen::graphics2d::Sprite& sprite);
};

} // namespace haylen::lua::graphics2d
