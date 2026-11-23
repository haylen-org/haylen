#include "2d/tiled/MapQueryBinding.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "2d/tiled/TiledBinding.hpp"
#include "haylen/2d/tiled/MapQuery.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Ray.hpp"
#include "math/RaycastBinding.hpp"

namespace haylen::lua::tiled {

const haylen::tiled::Map& MapQueryBinding::checkMap(lua_State* L) {
    return lua::Userdata::check<haylen::tiled::MapRenderer>(L, 1).getMap();
}

// Casts a ray over a tile layer with `raycastTiles(layer, x1, y1, x2, y2[, solid])`, where `solid(gid)` can tell which tiles block it.
int MapQueryBinding::raycastTiles(lua_State* L) {
    const haylen::tiled::Map& map = checkMap(L);
    const std::string layer = lua::Stack::read<std::string>(L, 2);
    const haylen::math::Ray ray = haylen::math::Ray::between({lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, {lua::Stack::read<float>(L, 5), lua::Stack::read<float>(L, 6)});
    haylen::tiled::MapQuery::SolidTile solid;
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

    const std::optional<haylen::tiled::MapQuery::TileHit> hit = haylen::tiled::MapQuery(map).castTiles(layer, ray, solid);
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }

    lua::math::RaycastBinding::pushHit(L, {.point = hit->point, .normal = hit->normal, .distance = hit->distance}, ray.length);
    lua_pushinteger(L, hit->column);
    lua_setfield(L, -2, "column");
    lua_pushinteger(L, hit->row);
    lua_setfield(L, -2, "row");
    lua_pushinteger(L, static_cast<lua_Integer>(hit->gid));
    lua_setfield(L, -2, "gid");
    return 1;
}

// Casts a ray against objects with `raycastObjects(layer, x1, y1, x2, y2)`, where a `nil` layer means every object layer.
int MapQueryBinding::raycastObjects(lua_State* L) {
    const haylen::tiled::Map& map = checkMap(L);
    const std::string layer = lua_isnoneornil(L, 2) ? std::string{} : lua::Stack::read<std::string>(L, 2);
    const haylen::math::Ray ray = haylen::math::Ray::between({lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, {lua::Stack::read<float>(L, 5), lua::Stack::read<float>(L, 6)});
    const std::optional<haylen::tiled::MapQuery::ObjectHit> hit = haylen::tiled::MapQuery(map).castObjects(layer, ray);
    if (!hit) {
        lua_pushnil(L);
        return 1;
    }

    lua::math::RaycastBinding::pushHit(L, {.point = hit->point, .normal = hit->normal, .distance = hit->distance}, ray.length);
    lua_pushinteger(L, static_cast<lua_Integer>(hit->object->id));
    lua_setfield(L, -2, "id");
    lua::Stack::push(L, hit->object->name);
    lua_setfield(L, -2, "name");
    lua::Stack::push(L, hit->object->type);
    lua_setfield(L, -2, "type");
    return 1;
}

// Lists the closed world outlines of the objects with `objectOutlines([layer])`, ready to become navigation mesh obstacles.
int MapQueryBinding::objectOutlines(lua_State* L) {
    const haylen::tiled::Map& map = checkMap(L);
    std::vector<std::vector<haylen::math::Vec2>> outlines;
    haylen::tiled::MapQuery(map).getOutlines(lua_isnoneornil(L, 2) ? std::string{} : lua::Stack::read<std::string>(L, 2), outlines);
    lua::Stack::push(L, outlines);
    return 1;
}

} // namespace haylen::lua::tiled
