#include "2d/procedural/PlacementLua.hpp"

#include <utility>

#include "2d/procedural/Procedural2DLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::procedural2d {

// Reads `{kind, center, radius, width, height, points, rotation}`, a circle when it names no kind.
Placement::Shape PlacementLua::readShape(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kShapeFields});
    Placement::Shape shape;
    lua::Table::readField(L, index, "kind", shape.kind);
    lua::Table::readField(L, index, "center", shape.center);
    lua::Table::readField(L, index, "radius", shape.radius);
    lua::Table::readField(L, index, "width", shape.size.x);
    lua::Table::readField(L, index, "height", shape.size.y);
    lua::Table::readField(L, index, "points", shape.points);
    lua::Table::readField(L, index, "rotation", shape.rotation);
    return shape;
}

Placement::Options PlacementLua::readOptions(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kFields});
    Placement::Options options;
    lua::Table::readField(L, index, "area", options.area);
    lua::Table::readField(L, index, "order", options.order);
    lua::Table::readField(L, index, "attempts", options.attempts);

    lua_getfield(L, index, "kinds");
    luaL_checktype(L, -1, LUA_TTABLE);
    const lua_Integer kinds = luaL_len(L, -1);
    for (lua_Integer place = 1; place <= kinds; ++place) {
        lua_rawgeti(L, -1, place);
        const int kind = lua_gettop(L);
        luaL_checktype(L, kind, LUA_TTABLE);
        lua::Table::checkFields(L, kind, {kKindFields});
        Placement::Kind& entry = options.kinds.emplace_back();
        lua::Table::readField(L, kind, "count", entry.count);
        lua::Table::readField(L, kind, "spacing", entry.spacing);
        lua::Table::readField(L, kind, "margin", entry.margin);
        lua::Table::readField(L, kind, "rotate", entry.rotate);
        lua_getfield(L, kind, "shape");
        entry.footprint = readShape(L, lua_gettop(L));
        lua_pop(L, 2);
    }
    lua_pop(L, 1);

    if (lua_getfield(L, index, "forbidden") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        const lua_Integer count = luaL_len(L, -1);
        for (lua_Integer area = 1; area <= count; ++area) {
            lua_rawgeti(L, -1, area);
            options.forbidden.push_back(readShape(L, lua_gettop(L)));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return options;
}

void PlacementLua::pushResult(lua_State* L, const Placement::Result& result) {
    lua_createtable(L, 0, 2);
    lua_createtable(L, static_cast<int>(result.entities.size()), 0);
    for (std::size_t index = 0; index < result.entities.size(); ++index) {
        const Placement::Entity& entity = result.entities[index];
        lua_createtable(L, 0, 4);
        lua_pushinteger(L, static_cast<lua_Integer>(entity.kind) + 1);
        lua_setfield(L, -2, "kind");
        lua::Stack::push(L, entity.position.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, entity.position.y);
        lua_setfield(L, -2, "y");
        lua::Stack::push(L, entity.rotation);
        lua_setfield(L, -2, "rotation");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "entities");

    lua_createtable(L, static_cast<int>(result.missing.size()), 0);
    for (std::size_t kind = 0; kind < result.missing.size(); ++kind) {
        lua_pushinteger(L, static_cast<lua_Integer>(result.missing[kind]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(kind + 1));
    }
    lua_setfield(L, -2, "missing");
}

int PlacementLua::place(lua_State* L) {
    const Placement::Options options = readOptions(L, 1);
    math::Random local(0);
    math::Random& random = Procedural2DLua::readGenerator(L, 1, local);
    pushResult(L, Placement::place(options, random));
    return 1;
}

int PlacementLua::placeAsync(lua_State* L) {
    Placement::Options options = readOptions(L, 1);
    math::Random local(0);
    math::Random random = Procedural2DLua::readGenerator(L, 1, local);
    // clang-format off
    return Procedural2DLua::spawn(L, [options = std::move(options), random]() mutable {
        return Placement::place(options, random);
    }, [](lua_State* state, const Placement::Result& result) { pushResult(state, result); });
    // clang-format on
}

void PlacementLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"place", &lua::Binding::native<&place>},
        {"placeAsync", &lua::Binding::native<&placeAsync>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::procedural2d
