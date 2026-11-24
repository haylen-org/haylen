#pragma once

#include <lua.hpp>

#include <array>
#include <string_view>

#include "haylen/lua/Type.hpp"
#include "haylen/math/Noise3D.hpp"

namespace haylen::lua {

template <> struct Type<haylen::math::Noise3D> {
    static constexpr const char* name = "haylen.Noise3D";
    using Storage = haylen::math::Noise3D;
};

} // namespace haylen::lua

namespace haylen::lua::math {

// Installs the `Noise3D` class of `haylen.math`, the noise of three and four dimensions, and its constructor.
class Noise3DBinding final {
  public:
    static void install(lua_State* L);

    // Sets the constructor `noise3d` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 6> kFractalFields{"basis", "fractal", "octaves", "lacunarity", "gain", "pingPongStrength"};

    static int newNoise(lua_State* L);
    static int perlin(lua_State* L);
    static int simplex(lua_State* L);
    static int value(lua_State* L);
    static int cellular(lua_State* L);
    static int fractal(lua_State* L);
    static int warp(lua_State* L);

    // Whether the argument after `z` is a number, which makes the sample 4D with that number as `w`.
    [[nodiscard]] static bool hasW(lua_State* L);
    [[nodiscard]] static haylen::math::Vec3 readPoint(lua_State* L);
    [[nodiscard]] static haylen::math::Noise3D::FractalOptions readFractal(lua_State* L, int index);
};

} // namespace haylen::lua::math
