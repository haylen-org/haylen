#pragma once

#include <lua.hpp>

#include <array>
#include <string_view>

namespace haylen::lua::math {

// Installs the `colorSpace` and `photometry` tables of `haylen.math`, the conversions of colors between linear light and the signal of each screen and the physical units of light.
class ColorSpaceBinding final {
  public:
    // Sets `colorSpace` and `photometry` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kSignalFields{"referenceWhite"};

    static int encode(lua_State* L);
    static int decode(lua_State* L);
    [[nodiscard]] static float readReferenceWhite(lua_State* L, int index);
};

} // namespace haylen::lua::math
