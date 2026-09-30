#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::ai {

// Installs the `InfluenceMap` class of `haylen.ai`.
class InfluenceMapLua final {
  public:
    static void install(lua_State* L);

    // Sets `newInfluenceMap` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 5> kMapFields{"columns", "rows", "cellSize", "x", "y"};

    // Pushes `x`, `y` and `value` of a spot, or `nil` when there is none.
    static int pushSpot(lua_State* L, bool highest);

    static int newMap(lua_State* L);
    static int stamp(lua_State* L);
    static int propagate(lua_State* L);
    static int scale(lua_State* L);
    static int add(lua_State* L);
    static int fill(lua_State* L);
    static int get(lua_State* L);
    static int set(lua_State* L);
    static int sample(lua_State* L);
    static int findHighest(lua_State* L);
    static int findLowest(lua_State* L);
    static int cellCenter(lua_State* L);
    static int values(lua_State* L);
    static int columns(lua_State* L);
    static int rows(lua_State* L);
    static int cellSize(lua_State* L);
    static int origin(lua_State* L);
};

} // namespace haylen::ai
