#include "2d/lighting/OccluderLua.hpp"

#include <stdexcept>
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

void OccluderLua::readPoints(lua_State* L, int index, std::vector<math::Vec2>& points) {
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    lua_rawgeti(L, table, 1);
    const bool flat = lua_type(L, -1) == LUA_TNUMBER;
    lua_pop(L, 1);
    if (!flat) {
        lua::Stack::readInto(L, table, points);
        return;
    }

    // The list may sit in a field of a table, so the error names the list instead of an argument.
    const auto count = static_cast<std::size_t>(luaL_len(L, table));
    points.resize(count / 2);
    bool numbers = count % 2 == 0;
    for (std::size_t point = 0; numbers && point < points.size(); ++point) {
        lua_rawgeti(L, table, static_cast<lua_Integer>(point * 2 + 1));
        lua_rawgeti(L, table, static_cast<lua_Integer>(point * 2 + 2));
        numbers = lua_type(L, -2) == LUA_TNUMBER && lua_type(L, -1) == LUA_TNUMBER;
        points[point] = {lua::Converter<float>::fromNumber(L, lua_tonumber(L, -2)), lua::Converter<float>::fromNumber(L, lua_tonumber(L, -1))};
        lua_pop(L, 2);
    }
    if (!numbers) {
        throw std::invalid_argument("A flat list of points needs two numbers per point.");
    }
}

void OccluderLua::read(lua_State* L, int index, Occluder& occluder) {
    if (const Occluder* found = lua::Userdata::test<Occluder>(L, index)) {
        occluder = *found;
        return;
    }

    // The table starts from the defaults, while the points keep the storage they had.
    occluder = Occluder{.points = std::move(occluder.points)};
    occluder.points.clear();
    if (lua_isnoneornil(L, index)) {
        return;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    lua::Table::checkFields(L, table, {kFields});
    lua_getfield(L, table, "points");
    if (!lua_isnil(L, -1)) {
        readPoints(L, -1, occluder.points);
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
}

void OccluderLua::pushList(lua_State* L, std::vector<Occluder> occluders) {
    lua_createtable(L, static_cast<int>(occluders.size()), 0);
    for (std::size_t index = 0; index < occluders.size(); ++index) {
        lua::Userdata::emplace<Occluder>(L, std::move(occluders[index]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

int OccluderLua::newOccluder(lua_State* L) {
    Occluder occluder;
    read(L, 1, occluder);
    lua::Userdata::emplace<Occluder>(L, std::move(occluder));
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
    readPoints(L, 3, lua::Userdata::check<Occluder>(L, 1).points);
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
