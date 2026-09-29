#include "2d/lighting/OccluderLua.hpp"

#include <utility>

#include "2d/physics/Physics2DLua.hpp"
#include "2d/tiled/TiledLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lighting2d {

std::vector<math::Vec2> OccluderLua::readPoints(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    lua_rawgeti(L, table, 1);
    const bool flat = lua_type(L, -1) == LUA_TNUMBER;
    lua_pop(L, 1);
    if (!flat) {
        return lua::Stack::read<std::vector<math::Vec2>>(L, table);
    }

    const auto count = static_cast<std::size_t>(luaL_len(L, table));
    luaL_argcheck(L, count % 2 == 0, index, "a flat list of points needs two numbers per point");
    std::vector<math::Vec2> points(count / 2);
    for (std::size_t point = 0; point < points.size(); ++point) {
        lua_rawgeti(L, table, static_cast<lua_Integer>(point * 2 + 1));
        lua_rawgeti(L, table, static_cast<lua_Integer>(point * 2 + 2));
        points[point] = {static_cast<float>(luaL_checknumber(L, -2)), static_cast<float>(luaL_checknumber(L, -1))};
        lua_pop(L, 2);
    }
    return points;
}

Occluder OccluderLua::read(lua_State* L, int index) {
    if (const Occluder* occluder = lua::Userdata::test<Occluder>(L, index)) {
        return *occluder;
    }
    Occluder occluder;
    if (lua_isnoneornil(L, index)) {
        return occluder;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    lua::Table::checkFields(L, table, {kFields});
    lua_getfield(L, table, "points");
    if (!lua_isnil(L, -1)) {
        occluder.points = readPoints(L, -1);
    }
    lua_pop(L, 1);
    lua::Table::readField(L, table, "closed", occluder.closed);
    lua::Table::readField(L, table, "cull", occluder.cull);
    lua::Table::readField(L, table, "mask", occluder.mask);
    lua::Table::readField(L, table, "position", occluder.position);
    lua::Table::readField(L, table, "x", occluder.position.x);
    lua::Table::readField(L, table, "y", occluder.position.y);
    lua::Table::readField(L, table, "rotation", occluder.rotation);
    lua::Table::readField(L, table, "scaleX", occluder.scale.x);
    lua::Table::readField(L, table, "scaleY", occluder.scale.y);
    return occluder;
}

void OccluderLua::pushList(lua_State* L, std::vector<Occluder> occluders) {
    lua_createtable(L, static_cast<int>(occluders.size()), 0);
    for (std::size_t index = 0; index < occluders.size(); ++index) {
        lua::Userdata::emplace<Occluder>(L, std::move(occluders[index]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

int OccluderLua::newOccluder(lua_State* L) {
    lua::Userdata::emplace<Occluder>(L, read(L, 1));
    return 1;
}

int OccluderLua::fromBody(lua_State* L) {
    const physics2d::World& world = lua::Userdata::check<physics2d::World>(L, 1);
    const physics2d::Body& body = lua::Userdata::check<physics2d::ScriptedHandle<physics2d::Body>>(L, 2).handle;
    pushList(L, Occluder::fromBody(world, body));
    return 1;
}

int OccluderLua::fromMap(lua_State* L) {
    const tiled::MapRenderer& map = lua::Userdata::check<tiled::MapRenderer>(L, 1);
    pushList(L, Occluder::fromMap(map, lua_isnoneornil(L, 2) ? std::string_view{} : lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int OccluderLua::getPoints(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Occluder>(L, 1).points);
    return 1;
}

int OccluderLua::setPoints(lua_State* L) {
    lua::Userdata::check<Occluder>(L, 1).points = readPoints(L, 3);
    return 0;
}

int OccluderLua::worldPoints(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Occluder>(L, 1).getWorldPoints());
    return 1;
}

void OccluderLua::install(lua_State* L) {
    lua::ClassBuilder<Occluder>(L).property("points", &getPoints, &lua::Binding::native<&setPoints>).field<&Occluder::closed>("closed").field<&Occluder::cull>("cull").field<&Occluder::mask>("mask").nestedField<&Occluder::position, &math::Vec2::x>("x").nestedField<&Occluder::position, &math::Vec2::y>("y").field<&Occluder::position>("position").field<&Occluder::rotation>("rotation").nestedField<&Occluder::scale, &math::Vec2::x>("scaleX").nestedField<&Occluder::scale, &math::Vec2::y>("scaleY").function("worldPoints", &worldPoints).install();
}

} // namespace haylen::lighting2d
