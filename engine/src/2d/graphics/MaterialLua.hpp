#pragma once

struct lua_State;

namespace haylen::graphics2d {

class Material;

// Installs the `Material` class of `haylen.graphics2d`, whose uniforms and textures Lua sets by name.
class MaterialLua final {
  public:
    static void install(lua_State* L);

    // Creates a material with `newMaterial(shader, {name = value, ...})`.
    static int newMaterial(lua_State* L);

  private:
    // Sets a uniform or texture from the Lua value at index: a number, a `Vec2`, a `Color` or color string, a `Transform`, a `Texture`, or a list of numbers.
    static void setValue(lua_State* L, Material& material, int nameIndex, int valueIndex);

    static int set(lua_State* L);
    static int get(lua_State* L);
    static int shader(lua_State* L);
};

} // namespace haylen::graphics2d
