#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::tiled {

// The functions of `haylen.tiled` that build maps at run time: `tiled.newMap` and the methods of `MapRenderer` that add and remove tilesets, layers and chunks, fill layers from grids, paint terrain and write the map as Tiled JSON.
class MapBuilderLua final {
  public:
    static int newMap(lua_State* L);
    static int addTileset(lua_State* L);
    static int addLayer(lua_State* L);
    static int removeLayer(lua_State* L);
    static int fill(lua_State* L);
    static int paintTerrain(lua_State* L);
    static int setChunk(lua_State* L);
    static int removeChunk(lua_State* L);
    static int toJson(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 11> kMapFields{"orientation", "renderOrder", "width", "height", "tileWidth", "tileHeight", "infinite", "staggerAxis", "staggerIndex", "hexSideLength", "backgroundColor"};
    static constexpr std::array<std::string_view, 13> kTilesetFields{"name", "image", "texture", "tileWidth", "tileHeight", "margin", "spacing", "tileCount", "filter", "wrap", "offsetX", "offsetY", "wangSets"};
    static constexpr std::array<std::string_view, 4> kWangSetFields{"name", "type", "colors", "tiles"};
    static constexpr std::array<std::string_view, 2> kWangTileFields{"tile", "wangId"};
    static constexpr std::array<std::string_view, 10> kLayerFields{"name", "kind", "visible", "opacity", "offsetX", "offsetY", "parallaxX", "parallaxY", "tint", "blend"};
};

} // namespace haylen::tiled
