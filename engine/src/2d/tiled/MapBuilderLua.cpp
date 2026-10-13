#include "2d/tiled/MapBuilderLua.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "2d/spatial/GridLua.hpp"
#include "2d/tiled/TiledLua.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::tiled {

// Creates an empty map with `tiled.newMap({orientation, width, height, tileWidth, tileHeight, ...})`, which `tiled.newMapRenderer` plays.
int MapBuilderLua::newMap(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kMapFields});
    auto map = std::make_shared<Map>();
    std::string orientation = "orthogonal";
    std::string renderOrder = "right-down";
    std::string staggerAxis = "y";
    std::string staggerIndex = "odd";
    lua::Table::readField(L, 1, "orientation", orientation);
    lua::Table::readField(L, 1, "renderOrder", renderOrder);
    lua::Table::readField(L, 1, "width", map->width);
    lua::Table::readField(L, 1, "height", map->height);
    lua::Table::readField(L, 1, "tileWidth", map->tileSize.x);
    lua::Table::readField(L, 1, "tileHeight", map->tileSize.y);
    lua::Table::readField(L, 1, "infinite", map->infinite);
    lua::Table::readField(L, 1, "staggerAxis", staggerAxis);
    lua::Table::readField(L, 1, "staggerIndex", staggerIndex);
    lua::Table::readField(L, 1, "hexSideLength", map->hexSideLength);
    if (lua_getfield(L, 1, "backgroundColor") != LUA_TNIL) {
        map->backgroundColor = lua::Stack::read<math::Color>(L, -1);
    }
    lua_pop(L, 1);

    const std::optional<Map::Orientation> parsedOrientation = Map::orientationFromName(orientation);
    const std::optional<Map::RenderOrder> parsedOrder = Map::renderOrderFromName(renderOrder);
    if (!parsedOrientation || !parsedOrder) {
        return luaL_error(L, "A map has the orientation \"orthogonal\", \"isometric\", \"staggered\", \"hexagonal\" or \"oblique\" and the render order \"right-down\", \"right-up\", \"left-down\" or \"left-up\".");
    }
    if ((!map->infinite && (map->width < 1 || map->height < 1)) || !(map->tileSize.x > 0.0F && map->tileSize.y > 0.0F)) {
        return luaL_error(L, "A map needs a positive tile size, and a finite map a positive width and height in cells.");
    }
    if ((staggerAxis != "x" && staggerAxis != "y") || (staggerIndex != "odd" && staggerIndex != "even")) {
        return luaL_error(L, "A map staggers along the axis \"x\" or \"y\" from the \"odd\" or \"even\" cells.");
    }
    map->orientation = *parsedOrientation;
    map->renderOrder = *parsedOrder;
    map->staggerX = staggerAxis == "x";
    map->staggerEven = staggerIndex == "even";
    TiledLua::pushMap(L, std::move(map));
    return 1;
}

// Adds a tileset cut from an image with `map:addTileset({name, image or texture, tileWidth, tileHeight, ...})` and returns its first global tile id.
int MapBuilderLua::addTileset(lua_State* L) {
    MapRenderer& renderer = lua::Userdata::check<MapRenderer>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kTilesetFields});
    auto tileset = std::make_shared<Tileset>();
    lua::Table::readField(L, 2, "name", tileset->name);
    lua::Table::readField(L, 2, "tileWidth", tileset->tileSize.x);
    lua::Table::readField(L, 2, "tileHeight", tileset->tileSize.y);
    lua::Table::readField(L, 2, "margin", tileset->margin);
    lua::Table::readField(L, 2, "spacing", tileset->spacing);
    lua::Table::readField(L, 2, "offsetX", tileset->tileOffset.x);
    lua::Table::readField(L, 2, "offsetY", tileset->tileOffset.y);
    if (!(tileset->tileSize.x > 0.0F && tileset->tileSize.y > 0.0F)) {
        return luaL_error(L, "A tileset needs a positive tile width and height.");
    }

    // The image of a path comes through the asset cache, and a texture made in code keeps the path it was loaded from, if any.
    if (lua_getfield(L, 2, "texture") != LUA_TNIL) {
        tileset->texture = lua::Stack::read<graphics::Texture>(L, -1);
        tileset->image = tileset->texture.getResource()->label;
    } else {
        lua_getfield(L, 2, "image");
        if (!lua_isstring(L, -1)) {
            return luaL_error(L, "A tileset needs the package path of its \"image\" or a \"texture\".");
        }
        tileset->image = lua::Stack::read<std::string>(L, -1);
        lua_pop(L, 1);
        tileset->texture = lua::Runtime::getEngine(L).getAssets().texture(tileset->image, lua::TypeConverter::readTextureOptions(L, 2, {kTilesetFields}));
    }
    lua_pop(L, 1);
    tileset->imageSize = tileset->texture.getSize();
    const float stepX = tileset->tileSize.x + static_cast<float>(tileset->spacing);
    const float stepY = tileset->tileSize.y + static_cast<float>(tileset->spacing);
    tileset->columns = static_cast<int>(std::floor((tileset->imageSize.x - static_cast<float>(tileset->margin * 2) + static_cast<float>(tileset->spacing)) / stepX));
    const int rows = static_cast<int>(std::floor((tileset->imageSize.y - static_cast<float>(tileset->margin * 2) + static_cast<float>(tileset->spacing)) / stepY));
    tileset->tileCount = tileset->columns * rows;
    lua::Table::readField(L, 2, "tileCount", tileset->tileCount);

    if (lua_getfield(L, 2, "wangSets") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int sets = lua_gettop(L);
        for (lua_Integer index = 1; index <= static_cast<lua_Integer>(luaL_len(L, sets)); ++index) {
            lua_rawgeti(L, sets, index);
            const int entry = lua_gettop(L);
            luaL_checktype(L, entry, LUA_TTABLE);
            lua::Table::checkFields(L, entry, {kWangSetFields});
            WangSet set{.kind = "corner"};
            lua::Table::readField(L, entry, "name", set.name);
            lua::Table::readField(L, entry, "type", set.kind);
            if (set.kind != "corner" && set.kind != "edge" && set.kind != "mixed") {
                return luaL_error(L, "A Wang set has the type \"corner\", \"edge\" or \"mixed\".");
            }
            std::vector<std::string> colors;
            lua::Table::readField(L, entry, "colors", colors);
            for (std::string& color : colors) {
                set.colors.push_back({.name = std::move(color)});
            }
            if (lua_getfield(L, entry, "tiles") != LUA_TNIL) {
                const int tiles = lua_gettop(L);
                for (lua_Integer place = 1; place <= static_cast<lua_Integer>(luaL_len(L, tiles)); ++place) {
                    lua_rawgeti(L, tiles, place);
                    lua::Table::checkFields(L, -1, {kWangTileFields});
                    std::uint32_t tile = 0;
                    std::vector<int> wangId;
                    lua::Table::readField(L, -1, "tile", tile);
                    lua::Table::readField(L, -1, "wangId", wangId);
                    if (wangId.size() != 8) {
                        return luaL_error(L, "A tile of a Wang set needs \"wangId\", the eight colors of its edges and corners from the top clockwise.");
                    }
                    WangSet::Tile parsed{.tileId = tile};
                    for (std::size_t corner = 0; corner < 8; ++corner) {
                        parsed.wangId[corner] = static_cast<std::uint8_t>(wangId[corner]);
                    }
                    set.tiles.push_back(parsed);
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
            tileset->wangSets.push_back(std::move(set));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    lua::Stack::push(L, renderer.addTileset(std::move(tileset)));
    return 1;
}

// Adds a layer on top with `map:addLayer({name, kind = 'tile', ...})`.
int MapBuilderLua::addLayer(lua_State* L) {
    MapRenderer& renderer = lua::Userdata::check<MapRenderer>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kLayerFields});
    Layer layer;
    std::string kind = "tile";
    lua::Table::readField(L, 2, "name", layer.name);
    lua::Table::readField(L, 2, "kind", kind);
    lua::Table::readField(L, 2, "visible", layer.visible);
    lua::Table::readField(L, 2, "opacity", layer.opacity);
    lua::Table::readField(L, 2, "offsetX", layer.offset.x);
    lua::Table::readField(L, 2, "offsetY", layer.offset.y);
    lua::Table::readField(L, 2, "parallaxX", layer.parallax.x);
    lua::Table::readField(L, 2, "parallaxY", layer.parallax.y);
    lua::Table::readField(L, 2, "tint", layer.tint);
    lua::Table::readField(L, 2, "blend", layer.blend);
    if (kind != "tile" && kind != "object") {
        return luaL_error(L, "A layer built in code has the kind \"tile\" or \"object\".");
    }
    layer.kind = kind == "tile" ? Layer::Kind::Tile : Layer::Kind::Object;
    renderer.addLayer(std::move(layer));
    return 0;
}

int MapBuilderLua::removeLayer(lua_State* L) {
    lua::Userdata::check<MapRenderer>(L, 1).removeLayer(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

// Fills cells with `map:fill(layer, grid, firstGid, column, row)`, where the column and the row count from 0 like the cells of the map.
int MapBuilderLua::fill(lua_State* L) {
    MapRenderer& renderer = lua::Userdata::check<MapRenderer>(L, 1);
    const spatial2d::CellGrid& grid = lua::Userdata::check<spatial2d::CellGrid>(L, 3);
    renderer.fill(lua::Stack::read<std::string_view>(L, 2), grid, static_cast<std::uint32_t>(luaL_optinteger(L, 4, 1)), static_cast<int>(luaL_optinteger(L, 5, 0)), static_cast<int>(luaL_optinteger(L, 6, 0)));
    return 0;
}

int MapBuilderLua::paintTerrain(lua_State* L) {
    lua::Userdata::check<MapRenderer>(L, 1).paintTerrain(lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<std::string_view>(L, 3), static_cast<int>(luaL_checkinteger(L, 4)), static_cast<int>(luaL_checkinteger(L, 5)), static_cast<int>(luaL_checkinteger(L, 6)));
    return 0;
}

int MapBuilderLua::setChunk(lua_State* L) {
    MapRenderer& renderer = lua::Userdata::check<MapRenderer>(L, 1);
    const spatial2d::CellGrid& grid = lua::Userdata::check<spatial2d::CellGrid>(L, 5);
    renderer.setChunk(lua::Stack::read<std::string_view>(L, 2), static_cast<int>(luaL_checkinteger(L, 3)), static_cast<int>(luaL_checkinteger(L, 4)), grid, static_cast<std::uint32_t>(luaL_optinteger(L, 6, 1)));
    return 0;
}

int MapBuilderLua::removeChunk(lua_State* L) {
    lua::Userdata::check<MapRenderer>(L, 1).removeChunk(lua::Stack::read<std::string_view>(L, 2), static_cast<int>(luaL_checkinteger(L, 3)), static_cast<int>(luaL_checkinteger(L, 4)));
    return 0;
}

// Returns the Tiled JSON of the map as a table with `map:toJson(path)`, for a file at that package path, the path the map was loaded from by default.
int MapBuilderLua::toJson(lua_State* L) {
    const MapRenderer& renderer = lua::Userdata::check<MapRenderer>(L, 1);
    const std::string path = lua_isnoneornil(L, 2) ? renderer.getMap().path : lua::Stack::read<std::string>(L, 2);
    lua::JsonConverter::push(L, renderer.toJson(path));
    return 1;
}

} // namespace haylen::tiled
