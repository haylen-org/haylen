#include "2d/tiled/MapQueryLua.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "2d/tiled/TiledLua.hpp"
#include "haylen/2d/tiled/MapQuery.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Ray.hpp"
#include "math/RaycastLua.hpp"

namespace haylen::tiled {

const Map& MapQueryLua::checkMap(lua_State* L) {
    return lua::Userdata::check<MapRenderer>(L, 1).getMap();
}

// Casts a ray over a tile layer with raycastTiles(layer, x1, y1, x2, y2[, solid]), where solid(gid) can tell which tiles block it.
int MapQueryLua::raycastTiles(lua_State* L) {
    const Map& map = checkMap(L);
    const std::string layer = lua::Stack::read<std::string>(L, 2);
    const math::Ray ray = math::Ray::between({lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, {lua::Stack::read<float>(L, 5), lua::Stack::read<float>(L, 6)});
    MapQuery::SolidTile solid;
    if (!lua_isnoneornil(L, 7)) {
        luaL_checktype(L, 7, LUA_TFUNCTION);
        // clang-format off
        solid = [L](std::uint32_t gid) {
            lua_pushvalue(L, 7);
            lua_pushinteger(L, static_cast<lua_Integer>(gid));
            lua_call(L, 1, 1);
            const bool blocks = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            return blocks;
        };
        // clang-format on
    }

    const std::optional<MapQuery::TileHit> hit = MapQuery(map).castTiles(layer, ray, solid);
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }
    math::RaycastLua::pushHit(L, {.point = hit->point, .normal = hit->normal, .distance = hit->distance}, ray.length);
    lua_pushinteger(L, hit->column);
    lua_setfield(L, -2, "column");
    lua_pushinteger(L, hit->row);
    lua_setfield(L, -2, "row");
    lua_pushinteger(L, static_cast<lua_Integer>(hit->gid));
    lua_setfield(L, -2, "gid");
    return 1;
}

// Casts a ray against objects with raycastObjects(layer, x1, y1, x2, y2), where a nil layer means every object layer.
int MapQueryLua::raycastObjects(lua_State* L) {
    const Map& map = checkMap(L);
    const std::string layer = lua_isnoneornil(L, 2) ? std::string{} : lua::Stack::read<std::string>(L, 2);
    const math::Ray ray = math::Ray::between({lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, {lua::Stack::read<float>(L, 5), lua::Stack::read<float>(L, 6)});
    const std::optional<MapQuery::ObjectHit> hit = MapQuery(map).castObjects(layer, ray);
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }
    math::RaycastLua::pushHit(L, {.point = hit->point, .normal = hit->normal, .distance = hit->distance}, ray.length);
    lua_pushinteger(L, static_cast<lua_Integer>(hit->object->id));
    lua_setfield(L, -2, "id");
    lua::Stack::push(L, hit->object->name);
    lua_setfield(L, -2, "name");
    lua::Stack::push(L, hit->object->type);
    lua_setfield(L, -2, "type");
    return 1;
}

// Lists the closed world outlines of the objects with objectOutlines([layer]), ready to become navigation mesh obstacles.
int MapQueryLua::objectOutlines(lua_State* L) {
    const Map& map = checkMap(L);
    std::vector<std::vector<math::Vec2>> outlines;
    MapQuery(map).getOutlines(lua_isnoneornil(L, 2) ? std::string{} : lua::Stack::read<std::string>(L, 2), outlines);
    lua::Stack::push(L, outlines);
    return 1;
}

} // namespace haylen::tiled
