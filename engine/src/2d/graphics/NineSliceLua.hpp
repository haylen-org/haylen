#pragma once

struct lua_State;

namespace haylen::graphics2d {

// Installs the `NineSlice` class of `haylen.graphics2d`.
class NineSliceLua final {
  public:
    static void install(lua_State* L);

  private:
    static int getPieces(lua_State* L);
    static int setPieces(lua_State* L);
    static int getBorders(lua_State* L);
    static int isValid(lua_State* L);
    static int layout(lua_State* L);
};

} // namespace haylen::graphics2d
