#include "2d/tiled/TiledLua.hpp"

#include <string>
#include <utility>
#include <vector>

#include "2d/physics/Physics2DLua.hpp"
#include "2d/tiled/MapQueryLua.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/World.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::tiled {

void TiledLua::setNumber(lua_State* L, const char* key, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, -2, key);
}

void TiledLua::setInteger(lua_State* L, const char* key, lua_Integer value) {
    lua_pushinteger(L, value);
    lua_setfield(L, -2, key);
}

void TiledLua::setString(lua_State* L, const char* key, std::string_view value) {
    lua::Stack::push(L, value);
    lua_setfield(L, -2, key);
}

void TiledLua::setBoolean(lua_State* L, const char* key, bool value) {
    lua_pushboolean(L, value ? 1 : 0);
    lua_setfield(L, -2, key);
}

void TiledLua::setOptionalColor(lua_State* L, const char* key, const std::optional<math::Color>& color) {
    if (color) {
        lua::Stack::push(L, *color);
        lua_setfield(L, -2, key);
    }
}

void TiledLua::pushPropertyValue(lua_State* L, std::string_view type, const core::Json& value) {
    if (type == "color") {
        if (const std::optional<math::Color> color = math::Color::parse(value.get<std::string>())) {
            lua::Stack::push(L, *color);
            return;
        }
        lua_pushnil(L);
        return;
    }
    if (type != "list") {
        lua::JsonConverter::push(L, value);
        return;
    }

    luaL_checkstack(L, LUA_MINSTACK, "the Tiled property lists are nested too deeply");
    lua_createtable(L, static_cast<int>(value.size()), 0);
    for (std::size_t index = 0; index < value.size(); ++index) {
        const core::Json& item = value[index];
        pushPropertyValue(L, item.value("type", std::string("string")), item.value("value", core::Json()));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

void TiledLua::pushProperties(lua_State* L, const Properties& properties) {
    lua_createtable(L, 0, static_cast<int>(properties.getItems().size()));
    for (const Property& property : properties.getItems()) {
        pushPropertyValue(L, property.type, property.value);
        lua_setfield(L, -2, property.name.c_str());
    }
}

void TiledLua::pushPropertyTypes(lua_State* L, const Properties& properties) {
    lua_newtable(L);
    for (const Property& property : properties.getItems()) {
        if (!property.propertyType.empty()) {
            lua::Stack::push(L, property.propertyType);
            lua_setfield(L, -2, property.name.c_str());
        }
    }
}

void TiledLua::setProperties(lua_State* L, const Properties& properties) {
    pushProperties(L, properties);
    lua_setfield(L, -2, "properties");
    pushPropertyTypes(L, properties);
    lua_setfield(L, -2, "propertyTypes");
}

void TiledLua::pushPoints(lua_State* L, const std::vector<math::Vec2>& points) {
    lua_createtable(L, static_cast<int>(points.size()), 0);
    for (std::size_t index = 0; index < points.size(); ++index) {
        lua_createtable(L, 0, 2);
        setNumber(L, "x", points[index].x);
        setNumber(L, "y", points[index].y);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

void TiledLua::pushObject(lua_State* L, const Object& object) {
    lua_createtable(L, 0, 16);
    setInteger(L, "id", object.id);
    setString(L, "name", object.name);
    setString(L, "type", object.type);
    setNumber(L, "x", object.position.x);
    setNumber(L, "y", object.position.y);
    setNumber(L, "width", object.size.x);
    setNumber(L, "height", object.size.y);
    setNumber(L, "rotation", object.rotation);
    setBoolean(L, "visible", object.visible);
    setNumber(L, "opacity", object.opacity);
    setString(L, "shape", Object::shapeName(object.shape));
    setInteger(L, "gid", object.gid);
    setString(L, "template", object.templatePath);
    pushPoints(L, object.points);
    lua_setfield(L, -2, "points");
    setProperties(L, object.properties);
    if (object.shape == Object::Shape::Text) {
        lua_createtable(L, 0, 9);
        setString(L, "text", object.text.text);
        setString(L, "fontFamily", object.text.fontFamily);
        setNumber(L, "pixelSize", object.text.pixelSize);
        setBoolean(L, "wrap", object.text.wrap);
        lua::Stack::push(L, object.text.color);
        lua_setfield(L, -2, "color");
        setBoolean(L, "bold", object.text.bold);
        setBoolean(L, "italic", object.text.italic);
        setString(L, "horizontalAlign", object.text.horizontalAlign);
        setString(L, "verticalAlign", object.text.verticalAlign);
        lua_setfield(L, -2, "text");
    }
}

void TiledLua::pushObjects(lua_State* L, const std::vector<Object>& objects) {
    lua_createtable(L, static_cast<int>(objects.size()), 0);
    for (std::size_t index = 0; index < objects.size(); ++index) {
        pushObject(L, objects[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

// Infinite maps store tile layers as chunks, each with its cell rectangle and its gids row by row.
void TiledLua::pushChunks(lua_State* L, const std::vector<Layer::Chunk>& chunks) {
    lua_createtable(L, static_cast<int>(chunks.size()), 0);
    for (std::size_t index = 0; index < chunks.size(); ++index) {
        const Layer::Chunk& chunk = chunks[index];
        lua_createtable(L, 0, 5);
        setInteger(L, "x", chunk.x);
        setInteger(L, "y", chunk.y);
        setInteger(L, "width", chunk.width);
        setInteger(L, "height", chunk.height);
        lua::Stack::push(L, chunk.gids);
        lua_setfield(L, -2, "gids");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

void TiledLua::pushLayer(lua_State* L, const Layer& layer) {
    luaL_checkstack(L, LUA_MINSTACK, "the Tiled layer groups are nested too deeply");
    lua_createtable(L, 0, 20);
    setInteger(L, "id", layer.id);
    setString(L, "name", layer.name);
    setString(L, "type", layer.type);
    setString(L, "kind", Layer::kindName(layer.kind));
    setBoolean(L, "visible", layer.visible);
    setNumber(L, "opacity", layer.opacity);
    setString(L, "blend", graphics::BlendMode::name(layer.blend));
    setNumber(L, "offsetX", layer.offset.x);
    setNumber(L, "offsetY", layer.offset.y);
    setNumber(L, "parallaxX", layer.parallax.x);
    setNumber(L, "parallaxY", layer.parallax.y);
    lua::Stack::push(L, layer.tint);
    lua_setfield(L, -2, "tint");
    setProperties(L, layer.properties);
    switch (layer.kind) {
    case Layer::Kind::Tile:
        setInteger(L, "width", layer.width);
        setInteger(L, "height", layer.height);
        pushChunks(L, layer.chunks);
        lua_setfield(L, -2, "chunks");
        break;
    case Layer::Kind::Object:
        pushObjects(L, layer.objects);
        lua_setfield(L, -2, "objects");
        setBoolean(L, "indexDrawOrder", layer.indexDrawOrder);
        break;
    case Layer::Kind::Image:
        setString(L, "image", layer.image);
        setBoolean(L, "repeatX", layer.repeatX);
        setBoolean(L, "repeatY", layer.repeatY);
        lua::Stack::push(L, layer.imageSize);
        lua_setfield(L, -2, "imageSize");
        setOptionalColor(L, "transparentColor", layer.transparentColor);
        break;
    case Layer::Kind::Group:
        lua_createtable(L, static_cast<int>(layer.layers.size()), 0);
        for (std::size_t index = 0; index < layer.layers.size(); ++index) {
            pushLayer(L, layer.layers[index]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }
        lua_setfield(L, -2, "layers");
        break;
    }
}

void TiledLua::collectObjects(const std::vector<Layer>& layers, std::vector<Object>& objects) {
    for (const Layer& layer : layers) {
        objects.insert(objects.end(), layer.objects.begin(), layer.objects.end());
        collectObjects(layer.layers, objects);
    }
}

void TiledLua::pushTileset(lua_State* L, const Tileset& tileset, std::uint32_t firstGid) {
    lua_createtable(L, 0, 24);
    setString(L, "name", tileset.name);
    setString(L, "type", tileset.type);
    setString(L, "path", tileset.path);
    setInteger(L, "firstGid", firstGid);
    setNumber(L, "tileWidth", tileset.tileSize.x);
    setNumber(L, "tileHeight", tileset.tileSize.y);
    setInteger(L, "columns", tileset.columns);
    setInteger(L, "tileCount", tileset.tileCount);
    setInteger(L, "margin", tileset.margin);
    setInteger(L, "spacing", tileset.spacing);
    lua::Stack::push(L, tileset.tileOffset);
    lua_setfield(L, -2, "tileOffset");
    setString(L, "objectAlignment", tileset.objectAlignment);
    setBoolean(L, "renderGridSize", tileset.renderGridSize);
    setBoolean(L, "preserveAspect", tileset.preserveAspect);
    setString(L, "image", tileset.image);
    lua::Stack::push(L, tileset.imageSize);
    lua_setfield(L, -2, "imageSize");
    setOptionalColor(L, "transparentColor", tileset.transparentColor);
    if (tileset.texture.isValid()) {
        lua::Stack::push(L, tileset.texture);
        lua_setfield(L, -2, "texture");
    }
    setProperties(L, tileset.properties);

    lua_createtable(L, static_cast<int>(tileset.wangSets.size()), 0);
    for (std::size_t index = 0; index < tileset.wangSets.size(); ++index) {
        const WangSet& set = tileset.wangSets[index];
        lua_createtable(L, 0, 8);
        setString(L, "name", set.name);
        setString(L, "type", set.type);
        setString(L, "kind", set.kind);
        setInteger(L, "tile", set.tile);
        setProperties(L, set.properties);
        lua_createtable(L, static_cast<int>(set.colors.size()), 0);
        for (std::size_t color = 0; color < set.colors.size(); ++color) {
            lua_createtable(L, 0, 7);
            setString(L, "name", set.colors[color].name);
            setString(L, "type", set.colors[color].type);
            lua::Stack::push(L, set.colors[color].color);
            lua_setfield(L, -2, "color");
            setInteger(L, "tile", set.colors[color].tile);
            setNumber(L, "probability", set.colors[color].probability);
            setProperties(L, set.colors[color].properties);
            lua_rawseti(L, -2, static_cast<lua_Integer>(color + 1));
        }
        lua_setfield(L, -2, "colors");
        lua_createtable(L, static_cast<int>(set.tiles.size()), 0);
        for (std::size_t tile = 0; tile < set.tiles.size(); ++tile) {
            lua_createtable(L, 0, 2);
            setInteger(L, "tileId", set.tiles[tile].tileId);
            lua_createtable(L, 8, 0);
            for (std::size_t corner = 0; corner < 8; ++corner) {
                lua_pushinteger(L, set.tiles[tile].wangId[corner]);
                lua_rawseti(L, -2, static_cast<lua_Integer>(corner + 1));
            }
            lua_setfield(L, -2, "wangId");
            lua_rawseti(L, -2, static_cast<lua_Integer>(tile + 1));
        }
        lua_setfield(L, -2, "tiles");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "wangSets");
}

// Parallax follows the point the camera shows, and culling uses the area the active canvas shows, which covers a render target canvas too.
MapRenderer::View TiledLua::readView(lua_State* L, int index) {
    if (lua_isnoneornil(L, index)) {
        return {};
    }
    return {.center = lua::Userdata::check<graphics2d::Camera>(L, index).getRenderPosition(), .visible = lua::Runtime::getEngine(L).getRenderer2D().getCanvasBounds()};
}

// Reads {layer, depth, sortOffset, visibility, blend, x, y, ysort}, where x and y place the map origin in the world.
MapRenderer::DrawOptions TiledLua::readDrawOptions(lua_State* L, int index) {
    MapRenderer::DrawOptions options{.order = lua::TypeConverter::readDrawOrder(L, index, {kMapDrawFields})};
    if (!lua_isnoneornil(L, index)) {
        lua::Table::readField(L, index, "x", options.offset.x);
        lua::Table::readField(L, index, "y", options.offset.y);
        lua::Table::readField(L, index, "ysort", options.ysort);
    }
    return options;
}

MapRenderer& TiledLua::checkRenderer(lua_State* L) {
    return lua::Userdata::check<MapRenderer>(L, 1);
}

const Map& TiledLua::checkMap(lua_State* L) {
    return checkRenderer(L).getMap();
}

// Creates a playable map from a loaded map asset with newMapRenderer(asset).
int TiledLua::newMapRenderer(lua_State* L) {
    const Map& data = lua::Userdata::check<Map>(L, 1);
    lua::Userdata::emplace<MapRenderer>(L, std::make_shared<MapRenderer>(data, lua::Runtime::getEngine(L).getDefaultFont()));
    return 1;
}

int TiledLua::mapDraw(lua_State* L) {
    checkRenderer(L).draw(lua::Runtime::getEngine(L).getRenderer2D(), readView(L, 2), readDrawOptions(L, 3));
    return 0;
}

int TiledLua::mapDrawLayer(lua_State* L) {
    checkRenderer(L).drawLayer(lua::Runtime::getEngine(L).getRenderer2D(), lua::Stack::read<std::string_view>(L, 2), readView(L, 3), readDrawOptions(L, 4));
    return 0;
}

int TiledLua::mapUpdate(lua_State* L) {
    checkRenderer(L).update(lua::Stack::read<float>(L, 2));
    return 0;
}

int TiledLua::mapTileAt(lua_State* L) {
    lua::Stack::push(L, checkRenderer(L).getTile(lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<int>(L, 3), lua::Stack::read<int>(L, 4)));
    return 1;
}

int TiledLua::mapSetTile(lua_State* L) {
    checkRenderer(L).setTile(lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<int>(L, 3), lua::Stack::read<int>(L, 4), lua::Stack::read<std::uint32_t>(L, 5));
    return 0;
}

int TiledLua::mapSetLayerVisible(lua_State* L) {
    checkRenderer(L).setLayerVisible(lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<bool>(L, 3));
    return 0;
}

int TiledLua::mapCellToWorld(lua_State* L) {
    const math::Vec2 position = checkMap(L).cellToWorld(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3));
    lua::Stack::push(L, position.x);
    lua::Stack::push(L, position.y);
    return 2;
}

int TiledLua::mapWorldToCell(lua_State* L) {
    const std::array<int, 2> cell = checkMap(L).worldToCell({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    lua::Stack::push(L, cell[0]);
    lua::Stack::push(L, cell[1]);
    return 2;
}

int TiledLua::mapObjectToWorld(lua_State* L) {
    const math::Vec2 position = checkMap(L).objectToWorld({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    lua::Stack::push(L, position.x);
    lua::Stack::push(L, position.y);
    return 2;
}

int TiledLua::mapLayer(lua_State* L) {
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const Layer* layer = checkMap(L).findLayer(name);
    if (layer == nullptr) {
        return luaL_error(L, "Unknown layer: %s", std::string(name).c_str());
    }
    pushLayer(L, *layer);
    return 1;
}

int TiledLua::mapLayers(lua_State* L) {
    const std::vector<Layer>& layers = checkMap(L).layers;
    lua_createtable(L, static_cast<int>(layers.size()), 0);
    for (std::size_t index = 0; index < layers.size(); ++index) {
        pushLayer(L, layers[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

// Returns the objects of one layer, or of every object layer when no name is given.
int TiledLua::mapObjects(lua_State* L) {
    const Map& data = checkMap(L);
    if (lua_isnoneornil(L, 2)) {
        std::vector<Object> objects;
        collectObjects(data.layers, objects);
        pushObjects(L, objects);
        return 1;
    }
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const Layer* layer = data.findLayer(name);
    if (layer == nullptr || layer->kind != Layer::Kind::Object) {
        return luaL_error(L, "The map has no object layer named '%s'.", std::string(name).c_str());
    }
    pushObjects(L, layer->objects);
    return 1;
}

// Calls factories[class](object) for each object whose class has a factory, in one object layer with spawn(factories, layer) or in all of them, and returns the values the factories returned in map order. Spawned objects also carry their world position as worldX and worldY.
int TiledLua::mapSpawn(lua_State* L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    const std::string_view layer = lua_isnoneornil(L, 3) ? std::string_view{} : lua::Stack::read<std::string_view>(L, 3);
    std::vector<std::pair<const Object*, math::Vec2>> objects;
    checkRenderer(L).forEachObject(layer, [&objects](const Object& object, math::Vec2 position) { objects.emplace_back(&object, position); });

    // The factories run Lua, so they run after the walk over the map.
    lua_newtable(L);
    const int results = lua_gettop(L);
    lua_Integer count = 0;
    for (const auto& [object, position] : objects) {
        lua_getfield(L, 2, object->type.c_str());
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        if (!lua_isfunction(L, -1)) {
            return luaL_error(L, "The factory for the Tiled class '%s' is not a function.", object->type.c_str());
        }
        pushObject(L, *object);
        setNumber(L, "worldX", position.x);
        setNumber(L, "worldY", position.y);
        lua_call(L, 1, 1);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        lua_rawseti(L, results, ++count);
    }
    return 1;
}

// Describes a global tile id with its tileset, class, properties, animation and collision shapes, or returns nil for an empty cell or an id that no tileset holds.
int TiledLua::mapTileInfo(lua_State* L) {
    const Map& data = checkMap(L);
    const auto gid = lua::Stack::read<std::uint32_t>(L, 2);
    const Map::TilesetReference* reference = data.findTileset(gid);
    if (reference == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    const std::uint32_t localId = Map::tileId(gid) - reference->firstGid;
    const Tile* tile = reference->tileset->findTile(localId);
    lua_createtable(L, 0, 13);
    setInteger(L, "id", localId);
    setString(L, "tileset", reference->tileset->name);
    setBoolean(L, "flippedX", (gid & Map::kFlipHorizontal) != 0);
    setBoolean(L, "flippedY", (gid & Map::kFlipVertical) != 0);
    setBoolean(L, "flippedDiagonally", (gid & Map::kFlipDiagonal) != 0);
    lua::Stack::push(L, reference->tileset->getSource(localId));
    lua_setfield(L, -2, "source");
    setString(L, "type", tile != nullptr ? tile->type : std::string{});
    setString(L, "image", tile != nullptr ? tile->image : std::string{});
    setNumber(L, "probability", tile != nullptr ? tile->probability : 1.0F);
    setProperties(L, tile != nullptr ? tile->properties : Properties{});
    pushObjects(L, tile != nullptr ? tile->collision : std::vector<Object>{});
    lua_setfield(L, -2, "collision");
    lua_createtable(L, 0, 0);
    if (tile != nullptr) {
        for (std::size_t index = 0; index < tile->animation.size(); ++index) {
            lua_createtable(L, 0, 2);
            setInteger(L, "tileId", tile->animation[index].tileId);
            setNumber(L, "duration", tile->animation[index].duration);
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }
    }
    lua_setfield(L, -2, "animation");
    return 1;
}

int TiledLua::mapTilesets(lua_State* L) {
    const std::vector<Map::TilesetReference>& tilesets = checkMap(L).tilesets;
    lua_createtable(L, static_cast<int>(tilesets.size()), 0);
    for (std::size_t index = 0; index < tilesets.size(); ++index) {
        pushTileset(L, *tilesets[index].tileset, tilesets[index].firstGid);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

// Creates static bodies for the collision shapes of the map in a physics world and returns them.
int TiledLua::mapBuildCollision(lua_State* L) {
    physics2d::World& world = lua::Userdata::check<physics2d::World>(L, 2);
    physics2d::Physics2DLua::pushList(L, 2, checkRenderer(L).buildCollision(world));
    return 1;
}

int TiledLua::mapWidth(lua_State* L) {
    lua::Stack::push(L, checkMap(L).width);
    return 1;
}

int TiledLua::mapHeight(lua_State* L) {
    lua::Stack::push(L, checkMap(L).height);
    return 1;
}

int TiledLua::mapTileWidth(lua_State* L) {
    lua::Stack::push(L, checkMap(L).tileSize.x);
    return 1;
}

int TiledLua::mapTileHeight(lua_State* L) {
    lua::Stack::push(L, checkMap(L).tileSize.y);
    return 1;
}

int TiledLua::mapBounds(lua_State* L) {
    lua::Stack::push(L, checkMap(L).getPixelBounds());
    return 1;
}

int TiledLua::mapSkewX(lua_State* L) {
    lua::Stack::push(L, checkMap(L).skew.x);
    return 1;
}

int TiledLua::mapSkewY(lua_State* L) {
    lua::Stack::push(L, checkMap(L).skew.y);
    return 1;
}

int TiledLua::mapOrientation(lua_State* L) {
    lua::Stack::push(L, Map::orientationName(checkMap(L).orientation));
    return 1;
}

int TiledLua::mapBackgroundColor(lua_State* L) {
    const std::optional<math::Color>& color = checkMap(L).backgroundColor;
    if (!color) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, *color);
    return 1;
}

int TiledLua::mapType(lua_State* L) {
    lua::Stack::push(L, checkMap(L).type);
    return 1;
}

int TiledLua::mapProperties(lua_State* L) {
    pushProperties(L, checkMap(L).properties);
    return 1;
}

int TiledLua::mapPropertyTypes(lua_State* L) {
    pushPropertyTypes(L, checkMap(L).properties);
    return 1;
}

int TiledLua::mapInfinite(lua_State* L) {
    lua::Stack::push(L, checkMap(L).infinite);
    return 1;
}

int TiledLua::mapRenderOrder(lua_State* L) {
    lua::Stack::push(L, Map::renderOrderName(checkMap(L).renderOrder));
    return 1;
}

int TiledLua::mapHexSideLength(lua_State* L) {
    lua::Stack::push(L, checkMap(L).hexSideLength);
    return 1;
}

int TiledLua::mapStaggerX(lua_State* L) {
    lua::Stack::push(L, checkMap(L).staggerX);
    return 1;
}

int TiledLua::mapStaggerEven(lua_State* L) {
    lua::Stack::push(L, checkMap(L).staggerEven);
    return 1;
}

int TiledLua::mapParallaxOrigin(lua_State* L) {
    lua::Stack::push(L, checkMap(L).parallaxOrigin);
    return 1;
}

int TiledLua::mapPath(lua_State* L) {
    lua::Stack::push(L, checkMap(L).path);
    return 1;
}

int TiledLua::assetPath(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Map>(L, 1).path);
    return 1;
}

int TiledLua::tileId(lua_State* L) {
    lua::Stack::push(L, Map::tileId(lua::Stack::read<std::uint32_t>(L, 1)));
    return 1;
}

int TiledLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newMapRenderer", &lua::Binding::native<&newMapRenderer>},
        {"tileId", &tileId},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    const std::pair<const char*, std::uint32_t> flags[] = {{"flipHorizontal", Map::kFlipHorizontal}, {"flipVertical", Map::kFlipVertical}, {"flipDiagonal", Map::kFlipDiagonal}, {"rotateHexagonal", Map::kRotateHexagonal}, {"flagMask", Map::kFlagMask}};
    for (const auto& [name, value] : flags) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, name);
    }
    return 1;
}

void TiledLua::pushMap(lua_State* L, std::shared_ptr<Map> data) {
    lua::Userdata::emplace<Map>(L, std::move(data));
}

void TiledLua::pushWorld(lua_State* L, const World& world) {
    lua_createtable(L, static_cast<int>(world.maps.size()), 0);
    for (std::size_t index = 0; index < world.maps.size(); ++index) {
        const World::Placement& placement = world.maps[index];
        lua_createtable(L, 0, 5);
        setString(L, "path", placement.path);
        setNumber(L, "x", placement.bounds.x);
        setNumber(L, "y", placement.bounds.y);
        setNumber(L, "width", placement.bounds.width);
        setNumber(L, "height", placement.bounds.height);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

void TiledLua::install(lua_State* L) {
    lua::ClassBuilder<Map>(L).property("path", &assetPath).meta("__eq", &lua::Userdata::equal<Map>).install();

    lua::ClassBuilder<MapRenderer>(L).function("draw", &lua::Binding::native<&mapDraw>).function("drawLayer", &lua::Binding::native<&mapDrawLayer>).function("update", &lua::Binding::native<&mapUpdate>).function("tileAt", &lua::Binding::native<&mapTileAt>).function("setTile", &lua::Binding::native<&mapSetTile>).function("setLayerVisible", &lua::Binding::native<&mapSetLayerVisible>).function("cellToWorld", &lua::Binding::native<&mapCellToWorld>).function("worldToCell", &lua::Binding::native<&mapWorldToCell>).function("objectToWorld", &lua::Binding::native<&mapObjectToWorld>).function("layer", &lua::Binding::native<&mapLayer>).function("layers", &lua::Binding::native<&mapLayers>).function("objects", &lua::Binding::native<&mapObjects>).function("spawn", &lua::Binding::native<&mapSpawn>).function("tileInfo", &lua::Binding::native<&mapTileInfo>).function("tilesets", &lua::Binding::native<&mapTilesets>).function("buildCollision", &lua::Binding::native<&mapBuildCollision>).function("raycastTiles", &lua::Binding::native<&MapQueryLua::raycastTiles>).function("raycastObjects", &lua::Binding::native<&MapQueryLua::raycastObjects>).function("objectOutlines", &lua::Binding::native<&MapQueryLua::objectOutlines>).property("width", &mapWidth).property("height", &mapHeight).property("tileWidth", &mapTileWidth).property("tileHeight", &mapTileHeight).property("bounds", &mapBounds).property("orientation", &mapOrientation).property("skewX", &mapSkewX).property("skewY", &mapSkewY).property("backgroundColor", &mapBackgroundColor).property("type", &mapType).property("properties", &lua::Binding::native<&mapProperties>).property("propertyTypes", &mapPropertyTypes).property("infinite", &mapInfinite).property("renderOrder", &mapRenderOrder).property("hexSideLength", &mapHexSideLength).property("staggerX", &mapStaggerX).property("staggerEven", &mapStaggerEven).property("parallaxOrigin", &mapParallaxOrigin).property("path", &mapPath).install();

    lua::Binding::preload(L, "haylen.tiled", &open);
}

} // namespace haylen::tiled
