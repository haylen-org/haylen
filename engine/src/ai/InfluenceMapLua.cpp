#include "ai/InfluenceMapLua.hpp"

#include <lua.hpp>

#include <optional>

#include "haylen/ai/InfluenceMap.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<ai::InfluenceMap> {
    static constexpr const char* name = "haylen.InfluenceMap";
    using Storage = ai::InfluenceMap;
};

template <> struct EnumNames<ai::InfluenceMap::Falloff> {
    static std::optional<ai::InfluenceMap::Falloff> fromName(std::string_view name) {
        return ai::InfluenceMap::falloffFromName(name);
    }
    static std::string_view name(ai::InfluenceMap::Falloff value) {
        return ai::InfluenceMap::falloffName(value);
    }
};

} // namespace haylen::lua

namespace haylen::ai {

// Creates a map with newInfluenceMap({columns, rows, cellSize = 32, x = 0, y = 0}).
int InfluenceMapLua::newMap(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kMapFields});
    int columnCount = 0;
    int rowCount = 0;
    float size = 32.0F;
    math::Vec2 corner{};
    lua::Table::readField(L, 1, "columns", columnCount);
    lua::Table::readField(L, 1, "rows", rowCount);
    lua::Table::readField(L, 1, "cellSize", size);
    lua::Table::readField(L, 1, "x", corner.x);
    lua::Table::readField(L, 1, "y", corner.y);
    lua::Userdata::emplace<InfluenceMap>(L, columnCount, rowCount, size, corner);
    return 1;
}

// Adds influence with stamp(x, y, strength, radius, falloff = 'linear').
int InfluenceMapLua::stamp(lua_State* L) {
    InfluenceMap& map = lua::Userdata::check<InfluenceMap>(L, 1);
    const math::Vec2 center{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const auto falloff = lua_isnoneornil(L, 6) ? InfluenceMap::Falloff::Linear : lua::Stack::read<InfluenceMap::Falloff>(L, 6);
    map.stamp(center, lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5), falloff);
    return 0;
}

int InfluenceMapLua::propagate(lua_State* L) {
    lua::Userdata::check<InfluenceMap>(L, 1).propagate(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int InfluenceMapLua::scale(lua_State* L) {
    lua::Userdata::check<InfluenceMap>(L, 1).scale(lua::Stack::read<float>(L, 2));
    return 0;
}

int InfluenceMapLua::add(lua_State* L) {
    InfluenceMap& map = lua::Userdata::check<InfluenceMap>(L, 1);
    map.add(lua::Userdata::check<InfluenceMap>(L, 2), static_cast<float>(luaL_optnumber(L, 3, 1.0)));
    return 0;
}

int InfluenceMapLua::fill(lua_State* L) {
    lua::Userdata::check<InfluenceMap>(L, 1).fill(lua::Stack::read<float>(L, 2));
    return 0;
}

int InfluenceMapLua::get(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InfluenceMap>(L, 1).get(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)));
    return 1;
}

int InfluenceMapLua::set(lua_State* L) {
    lua::Userdata::check<InfluenceMap>(L, 1).set(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3), lua::Stack::read<float>(L, 4));
    return 0;
}

int InfluenceMapLua::sample(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InfluenceMap>(L, 1).sample({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

int InfluenceMapLua::pushSpot(lua_State* L, bool highest) {
    const InfluenceMap& map = lua::Userdata::check<InfluenceMap>(L, 1);
    const math::Vec2 center{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const auto radius = lua::Stack::read<float>(L, 4);
    const std::optional<InfluenceMap::Spot> spot = highest ? map.findHighest(center, radius) : map.findLowest(center, radius);
    if (!spot) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, spot->position.x);
    lua::Stack::push(L, spot->position.y);
    lua::Stack::push(L, spot->value);
    return 3;
}

int InfluenceMapLua::findHighest(lua_State* L) {
    return pushSpot(L, true);
}

int InfluenceMapLua::findLowest(lua_State* L) {
    return pushSpot(L, false);
}

int InfluenceMapLua::cellCenter(lua_State* L) {
    const math::Vec2 center = lua::Userdata::check<InfluenceMap>(L, 1).getCellCenter(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3));
    lua::Stack::push(L, center.x);
    lua::Stack::push(L, center.y);
    return 2;
}

// Returns every value row by row, which suits drawing the map for debugging.
int InfluenceMapLua::values(lua_State* L) {
    const std::span<const float> cells = lua::Userdata::check<InfluenceMap>(L, 1).getValues();
    lua_createtable(L, static_cast<int>(cells.size()), 0);
    for (std::size_t index = 0; index < cells.size(); ++index) {
        lua_pushnumber(L, cells[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int InfluenceMapLua::columns(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InfluenceMap>(L, 1).getColumns());
    return 1;
}

int InfluenceMapLua::rows(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InfluenceMap>(L, 1).getRows());
    return 1;
}

int InfluenceMapLua::cellSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InfluenceMap>(L, 1).getCellSize());
    return 1;
}

int InfluenceMapLua::origin(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InfluenceMap>(L, 1).getOrigin());
    return 1;
}

void InfluenceMapLua::install(lua_State* L) {
    lua::ClassBuilder<InfluenceMap>(L).function("stamp", &lua::Binding::native<&stamp>).function("propagate", &lua::Binding::native<&propagate>).function("scale", &lua::Binding::native<&scale>).function("add", &lua::Binding::native<&add>).function("fill", &lua::Binding::native<&fill>).function("get", &lua::Binding::native<&get>).function("set", &lua::Binding::native<&set>).function("sample", &lua::Binding::native<&sample>).function("findHighest", &lua::Binding::native<&findHighest>).function("findLowest", &lua::Binding::native<&findLowest>).function("cellCenter", &lua::Binding::native<&cellCenter>).function("values", &lua::Binding::native<&values>).property("columns", &columns).property("rows", &rows).property("cellSize", &cellSize).property("origin", &origin).install();
}

void InfluenceMapLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newMap>);
    lua_setfield(L, -2, "newInfluenceMap");
}

} // namespace haylen::ai
