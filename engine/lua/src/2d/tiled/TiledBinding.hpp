#pragma once

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/tiled/AutoMapper.hpp"
#include "haylen/2d/tiled/Layer.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "haylen/2d/tiled/Object.hpp"
#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/2d/tiled/Tileset.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lua {

template <> struct Type<haylen::tiled::Map> {
    static constexpr const char* name = "haylen.Map";
    using Storage = std::shared_ptr<haylen::tiled::Map>;
};

template <> struct Type<haylen::tiled::AutoMapper> {
    static constexpr const char* name = "haylen.AutoMapper";
    using Storage = std::shared_ptr<haylen::tiled::AutoMapper>;
};

template <> struct Type<haylen::tiled::MapRenderer> {
    static constexpr const char* name = "haylen.MapRenderer";
    using Storage = std::shared_ptr<haylen::tiled::MapRenderer>;
};

} // namespace haylen::lua

namespace haylen::tiled {
struct World;
}

namespace haylen::lua::tiled {
// Installs `haylen.tiled` with the `Map` asset class and the playable `MapRenderer` class. Map data crosses into Lua as plain tables.
class TiledBinding final {
  public:
    static void install(lua_State* L);

    // Pushes a loaded map, which the asset loader of Tiled maps hands to Lua.
    static void pushMap(lua_State* L, std::shared_ptr<haylen::tiled::Map> data);

    // Pushes the maps of a loaded world as `{path, x, y, width, height}` tables.
    static void pushWorld(lua_State* L, const haylen::tiled::World& world);

    // Pushes loaded automapping rules, whose `ruleMaps` lists `{path, filter}` for each rule map.
    static void pushAutoMapper(lua_State* L, std::shared_ptr<haylen::tiled::AutoMapper> mapper);

  private:
    static int autoMapperRuleMaps(lua_State* L);
    static constexpr std::array<std::string_view, 3> kMapDrawFields{"x", "y", "ysort"};

    static void setNumber(lua_State* L, const char* key, double value);
    static void setInteger(lua_State* L, const char* key, lua_Integer value);
    static void setString(lua_State* L, const char* key, std::string_view value);
    static void setBoolean(lua_State* L, const char* key, bool value);
    static void setOptionalColor(lua_State* L, const char* key, const std::optional<haylen::math::Color>& color);

    // Colors become `Color` values and unset colors become `nil`, lists become sequences of their item values, and class values become nested tables.
    static void pushPropertyValue(lua_State* L, std::string_view type, const haylen::core::Json& value);
    static void pushProperties(lua_State* L, const haylen::tiled::Properties& properties);

    // Pushes a table from property name to custom property type name, for the properties that have one.
    static void pushPropertyTypes(lua_State* L, const haylen::tiled::Properties& properties);

    // Sets the `properties` and `propertyTypes` fields of the table on top of the stack.
    static void setProperties(lua_State* L, const haylen::tiled::Properties& properties);
    static void pushPoints(lua_State* L, const std::vector<haylen::math::Vec2>& points);
    static void pushObject(lua_State* L, const haylen::tiled::Object& object);
    static void pushObjects(lua_State* L, const std::vector<haylen::tiled::Object>& objects);
    static void pushChunks(lua_State* L, const std::vector<haylen::tiled::Layer::Chunk>& chunks);
    static void pushLayer(lua_State* L, const haylen::tiled::Layer& layer);
    static void collectObjects(const std::vector<haylen::tiled::Layer>& layers, std::vector<haylen::tiled::Object>& objects);
    static void pushTileset(lua_State* L, const haylen::tiled::Tileset& tileset, std::uint32_t firstGid);
    [[nodiscard]] static haylen::tiled::MapRenderer::View readView(lua_State* L, int index);
    [[nodiscard]] static haylen::tiled::MapRenderer::DrawOptions readDrawOptions(lua_State* L, int index);
    [[nodiscard]] static haylen::tiled::MapRenderer& checkRenderer(lua_State* L);
    [[nodiscard]] static const haylen::tiled::Map& checkMap(lua_State* L);

    static int newMapRenderer(lua_State* L);
    static int mapDraw(lua_State* L);
    static int mapDrawLayer(lua_State* L);
    static int mapUpdate(lua_State* L);
    static int mapTile(lua_State* L);
    static int mapSetTile(lua_State* L);
    static int mapSetLayerVisible(lua_State* L);
    static int mapCellToWorld(lua_State* L);
    static int mapWorldToCell(lua_State* L);
    static int mapObjectToWorld(lua_State* L);
    static int mapLayer(lua_State* L);
    static int mapLayers(lua_State* L);
    static int mapObjects(lua_State* L);
    static int mapSpawn(lua_State* L);
    static int mapTileInfo(lua_State* L);
    static int mapTilesets(lua_State* L);
    static int mapBuildCollision(lua_State* L);
    static int mapWidth(lua_State* L);
    static int mapHeight(lua_State* L);
    static int mapTileWidth(lua_State* L);
    static int mapTileHeight(lua_State* L);
    static int mapPixelBounds(lua_State* L);
    static int mapSkewX(lua_State* L);
    static int mapSkewY(lua_State* L);
    static int mapOrientation(lua_State* L);
    static int mapBackgroundColor(lua_State* L);
    static int mapType(lua_State* L);
    static int mapProperties(lua_State* L);
    static int mapPropertyTypes(lua_State* L);
    static int mapInfinite(lua_State* L);
    static int mapRenderOrder(lua_State* L);
    static int mapHexSideLength(lua_State* L);
    static int mapStaggerX(lua_State* L);
    static int mapStaggerEven(lua_State* L);
    static int mapParallaxOrigin(lua_State* L);
    static int mapPath(lua_State* L);
    static int assetPath(lua_State* L);
    static int tileId(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::tiled
