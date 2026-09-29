#pragma once

#include <lua.hpp>

#include <array>
#include <string_view>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Adds the polygon and marchingSquares tables of haylen.math. Shapes cross into Lua as lists of outlines, each a list of points, and a single outline also works wherever a shape does.
class PolygonLua final {
  public:
    // Sets the tables on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

    // Reads a shape, or one outline as a shape with one outline.
    [[nodiscard]] static std::vector<std::vector<Vec2>> readShape(lua_State* L, int index);

  private:
    static constexpr std::array<std::string_view, 2> kOffsetFields{"join", "miterLimit"};
    static constexpr std::array<std::string_view, 3> kTraceFields{"threshold", "spacing", "origin"};
    static constexpr std::array<std::string_view, 2> kBitmapFields{"spacing", "origin"};

    [[nodiscard]] static bool isPoint(lua_State* L, int index);
    [[nodiscard]] static std::vector<std::vector<Vec2>> readClips(lua_State* L, int index);

    static int unite(lua_State* L);
    static int subtract(lua_State* L);
    static int intersect(lua_State* L);
    static int exclude(lua_State* L);
    static int offset(lua_State* L);
    static int simplify(lua_State* L);
    static int decompose(lua_State* L);
    static int area(lua_State* L);
    static int trace(lua_State* L);
    static int traceBitmap(lua_State* L);
};

} // namespace haylen::math
