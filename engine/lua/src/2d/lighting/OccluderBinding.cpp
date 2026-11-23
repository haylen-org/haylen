#include "2d/lighting/OccluderBinding.hpp"

#include <stdexcept>
#include <utility>

#include "2d/physics/Physics2DBinding.hpp"
#include "2d/tiled/TiledBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::lighting2d {

void OccluderBinding::readPoints(lua_State* L, int index, std::vector<haylen::math::Vec2>& points) {
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
    const auto count = static_cast<std::size_t>(lua_rawlen(L, table));
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

void OccluderBinding::read(lua_State* L, int index, haylen::lighting2d::Occluder& occluder) {
    if (const haylen::lighting2d::Occluder* found = lua::Userdata::test<haylen::lighting2d::Occluder>(L, index)) {
        occluder = *found;
        return;
    }

    // The table starts from the defaults, while the points keep the storage they had.
    occluder = haylen::lighting2d::Occluder{.points = std::move(occluder.points)};
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

void OccluderBinding::pushList(lua_State* L, std::vector<haylen::lighting2d::Occluder> occluders) {
    lua_createtable(L, static_cast<int>(occluders.size()), 0);
    for (std::size_t index = 0; index < occluders.size(); ++index) {
        lua::Userdata::emplace<haylen::lighting2d::Occluder>(L, std::move(occluders[index]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

int OccluderBinding::newOccluder(lua_State* L) {
    haylen::lighting2d::Occluder occluder;
    read(L, 1, occluder);
    lua::Userdata::emplace<haylen::lighting2d::Occluder>(L, std::move(occluder));
    return 1;
}

int OccluderBinding::fromBody(lua_State* L) {
    const haylen::physics2d::World& world = lua::Userdata::check<haylen::physics2d::World>(L, 1);
    const haylen::physics2d::Body& body = lua::Userdata::check<lua::physics2d::ScriptedHandle<haylen::physics2d::Body>>(L, 2).handle;
    pushList(L, haylen::lighting2d::Occluder::fromBody(world, body));
    return 1;
}

int OccluderBinding::fromMap(lua_State* L) {
    const haylen::tiled::MapRenderer& map = lua::Userdata::check<haylen::tiled::MapRenderer>(L, 1);
    pushList(L, haylen::lighting2d::Occluder::fromMap(map, lua_isnoneornil(L, 2) ? std::string_view{} : lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int OccluderBinding::getPoints(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::lighting2d::Occluder>(L, 1).points);
    return 1;
}

int OccluderBinding::setPoints(lua_State* L) {
    readPoints(L, 3, lua::Userdata::check<haylen::lighting2d::Occluder>(L, 1).points);
    return 0;
}

int OccluderBinding::worldPoints(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::lighting2d::Occluder>(L, 1).getWorldPoints());
    return 1;
}

void OccluderBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::lighting2d::Occluder>(L).property("points", &getPoints, &lua::Binding::native<&setPoints>).field<&haylen::lighting2d::Occluder::closed>("closed").field<&haylen::lighting2d::Occluder::cull>("cull").field<&haylen::lighting2d::Occluder::mask>("mask").nestedField<&haylen::lighting2d::Occluder::position, &haylen::math::Vec2::x>("x").nestedField<&haylen::lighting2d::Occluder::position, &haylen::math::Vec2::y>("y").field<&haylen::lighting2d::Occluder::position>("position").field<&haylen::lighting2d::Occluder::rotation>("rotation").nestedField<&haylen::lighting2d::Occluder::scale, &haylen::math::Vec2::x>("scaleX").nestedField<&haylen::lighting2d::Occluder::scale, &haylen::math::Vec2::y>("scaleY").function("worldPoints", &worldPoints).install();
}

} // namespace haylen::lua::lighting2d
