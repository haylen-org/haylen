#include "ai/InfluenceMapBinding.hpp"

#include <lua.hpp>

#include <optional>

#include "haylen/ai/InfluenceMap.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<haylen::ai::InfluenceMap> {
    static constexpr const char* name = "haylen.InfluenceMap";
    using Storage = haylen::ai::InfluenceMap;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::ai::InfluenceMap::Falloff> {
    static std::optional<haylen::ai::InfluenceMap::Falloff> fromName(std::string_view name) {
        return haylen::ai::InfluenceMap::falloffFromName(name);
    }
    static std::string_view name(haylen::ai::InfluenceMap::Falloff value) {
        return haylen::ai::InfluenceMap::falloffName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::ai {

// Creates a map with `newInfluenceMap({columns, rows, cellSize = 32, x = 0, y = 0})`.
int InfluenceMapBinding::newMap(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kMapFields});
    int columnCount = 0;
    int rowCount = 0;
    float size = 32.0F;
    haylen::math::Vec2 corner{};
    lua::Table::readField(L, 1, "columns", columnCount);
    lua::Table::readField(L, 1, "rows", rowCount);
    lua::Table::readField(L, 1, "cellSize", size);
    lua::Table::readField(L, 1, "x", corner.x);
    lua::Table::readField(L, 1, "y", corner.y);
    lua::Userdata::emplace<haylen::ai::InfluenceMap>(L, columnCount, rowCount, size, corner);
    return 1;
}

// Adds influence with `stamp(x, y, strength, radius, falloff = 'linear')`.
int InfluenceMapBinding::stamp(lua_State* L) {
    haylen::ai::InfluenceMap& map = lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1);
    const haylen::math::Vec2 center{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const auto falloff = lua_isnoneornil(L, 6) ? haylen::ai::InfluenceMap::Falloff::Linear : lua::Stack::read<haylen::ai::InfluenceMap::Falloff>(L, 6);
    map.stamp(center, lua::Stack::read<float>(L, 4), lua::Stack::read<float>(L, 5), falloff);
    return 0;
}

int InfluenceMapBinding::propagate(lua_State* L) {
    lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).propagate(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int InfluenceMapBinding::scale(lua_State* L) {
    lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).scale(lua::Stack::read<float>(L, 2));
    return 0;
}

int InfluenceMapBinding::add(lua_State* L) {
    haylen::ai::InfluenceMap& map = lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1);
    map.add(lua::Userdata::check<haylen::ai::InfluenceMap>(L, 2), lua::Converter<float>::readOr(L, 3, 1.0));
    return 0;
}

int InfluenceMapBinding::fill(lua_State* L) {
    lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).fill(lua::Stack::read<float>(L, 2));
    return 0;
}

int InfluenceMapBinding::get(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).get(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3)));
    return 1;
}

int InfluenceMapBinding::set(lua_State* L) {
    lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).set(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3), lua::Stack::read<float>(L, 4));
    return 0;
}

int InfluenceMapBinding::sample(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).sample({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

int InfluenceMapBinding::pushSpot(lua_State* L, bool highest) {
    const haylen::ai::InfluenceMap& map = lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1);
    const haylen::math::Vec2 center{lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)};
    const auto radius = lua::Stack::read<float>(L, 4);
    const std::optional<haylen::ai::InfluenceMap::Spot> spot = highest ? map.findHighest(center, radius) : map.findLowest(center, radius);
    if (!spot) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, spot->position.x);
    lua::Stack::push(L, spot->position.y);
    lua::Stack::push(L, spot->value);
    return 3;
}

int InfluenceMapBinding::findHighest(lua_State* L) {
    return pushSpot(L, true);
}

int InfluenceMapBinding::findLowest(lua_State* L) {
    return pushSpot(L, false);
}

int InfluenceMapBinding::cellCenter(lua_State* L) {
    const haylen::math::Vec2 center = lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).getCellCenter(lua::Stack::read<int>(L, 2), lua::Stack::read<int>(L, 3));
    lua::Stack::push(L, center.x);
    lua::Stack::push(L, center.y);
    return 2;
}

// Returns every value row by row, which suits drawing the map for debugging.
int InfluenceMapBinding::values(lua_State* L) {
    const std::span<const float> cells = lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).getValues();
    lua_createtable(L, static_cast<int>(cells.size()), 0);
    for (std::size_t index = 0; index < cells.size(); ++index) {
        lua_pushnumber(L, cells[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int InfluenceMapBinding::columns(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).getColumns());
    return 1;
}

int InfluenceMapBinding::rows(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).getRows());
    return 1;
}

int InfluenceMapBinding::cellSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).getCellSize());
    return 1;
}

int InfluenceMapBinding::origin(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::ai::InfluenceMap>(L, 1).getOrigin());
    return 1;
}

void InfluenceMapBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::ai::InfluenceMap>(L).function("stamp", &lua::Binding::native<&stamp>).function("propagate", &lua::Binding::native<&propagate>).function("scale", &lua::Binding::native<&scale>).function("add", &lua::Binding::native<&add>).function("fill", &lua::Binding::native<&fill>).function("get", &lua::Binding::native<&get>).function("set", &lua::Binding::native<&set>).function("sample", &lua::Binding::native<&sample>).function("findHighest", &lua::Binding::native<&findHighest>).function("findLowest", &lua::Binding::native<&findLowest>).function("cellCenter", &lua::Binding::native<&cellCenter>).function("values", &lua::Binding::native<&values>).property("columns", &columns).property("rows", &rows).property("cellSize", &cellSize).property("origin", &origin).install();
}

void InfluenceMapBinding::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newMap>);
    lua_setfield(L, -2, "newInfluenceMap");
}

} // namespace haylen::lua::ai
