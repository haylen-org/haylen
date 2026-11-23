#pragma once

#include <lua.hpp>

namespace haylen::tiled {
class Map;
}

namespace haylen::lua::tiled {
// Binds the ray casts and object outlines of `tiled::MapQuery` as methods of `MapRenderer`. Hits cross into Lua like the ray casts of `haylen.math`, with the cell or the object they hit.
class MapQueryBinding final {
  public:
    static int raycastTiles(lua_State* L);
    static int raycastObjects(lua_State* L);
    static int objectOutlines(lua_State* L);

  private:
    [[nodiscard]] static const haylen::tiled::Map& checkMap(lua_State* L);
};

} // namespace haylen::lua::tiled
