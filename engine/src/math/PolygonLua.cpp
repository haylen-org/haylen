#include "math/PolygonLua.hpp"

#include <cstdint>
#include <optional>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/MarchingSquares.hpp"
#include "haylen/math/Polygon.hpp"

namespace haylen::lua {

template <> struct EnumNames<math::Polygon::Join> {
    static std::optional<math::Polygon::Join> fromName(std::string_view name) {
        return math::Polygon::joinFromName(name);
    }
    static std::string_view name(math::Polygon::Join value) {
        return math::Polygon::joinName(value);
    }
};

} // namespace haylen::lua

namespace haylen::math {

// A point is a `Vec2`, a table with an `x` field or a table whose first value is a number.
bool PolygonLua::isPoint(lua_State* L, int index) {
    if (lua::Userdata::test<Vec2>(L, index) != nullptr) {
        return true;
    }
    if (!lua_istable(L, index)) {
        return false;
    }
    const int table = lua_absindex(L, index);
    const bool named = lua_getfield(L, table, "x") == LUA_TNUMBER;
    const bool listed = lua_rawgeti(L, table, 1) == LUA_TNUMBER;
    lua_pop(L, 2);
    return named || listed;
}

std::vector<std::vector<Vec2>> PolygonLua::readShape(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    lua_rawgeti(L, table, 1);
    const bool outline = isPoint(L, -1);
    lua_pop(L, 1);
    if (outline) {
        return {lua::Stack::read<std::vector<Vec2>>(L, table)};
    }
    return lua::Stack::read<std::vector<std::vector<Vec2>>>(L, table);
}

std::vector<std::vector<Vec2>> PolygonLua::readClips(lua_State* L, int index) {
    return lua_isnoneornil(L, index) ? std::vector<std::vector<Vec2>>{} : readShape(L, index);
}

int PolygonLua::unite(lua_State* L) {
    lua::Stack::push(L, Polygon::unite(readShape(L, 1), readClips(L, 2)));
    return 1;
}

int PolygonLua::subtract(lua_State* L) {
    lua::Stack::push(L, Polygon::subtract(readShape(L, 1), readShape(L, 2)));
    return 1;
}

int PolygonLua::intersect(lua_State* L) {
    lua::Stack::push(L, Polygon::intersect(readShape(L, 1), readShape(L, 2)));
    return 1;
}

int PolygonLua::exclude(lua_State* L) {
    lua::Stack::push(L, Polygon::exclude(readShape(L, 1), readShape(L, 2)));
    return 1;
}

// Grows a shape with `offset(shape, distance, {join = 'round', miterLimit = 2})`.
int PolygonLua::offset(lua_State* L) {
    const std::vector<std::vector<Vec2>> shape = readShape(L, 1);
    const auto distance = lua::Stack::read<float>(L, 2);
    Polygon::Join join = Polygon::Join::Round;
    float miterLimit = 2.0F;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kOffsetFields});
        lua::Table::readField(L, 3, "join", join);
        lua::Table::readField(L, 3, "miterLimit", miterLimit);
    }
    lua::Stack::push(L, Polygon::offset(shape, distance, join, miterLimit));
    return 1;
}

int PolygonLua::simplify(lua_State* L) {
    const auto points = lua::Stack::read<std::vector<Vec2>>(L, 1);
    const auto tolerance = lua::Stack::read<float>(L, 2);
    const bool closed = lua_isnoneornil(L, 3) || lua::Stack::read<bool>(L, 3);
    lua::Stack::push(L, Polygon::simplify(points, tolerance, closed));
    return 1;
}

int PolygonLua::decompose(lua_State* L) {
    const std::vector<std::vector<Vec2>> shape = readShape(L, 1);
    const std::size_t maxVertices = lua::Stack::read<std::optional<std::size_t>>(L, 2).value_or(8);
    lua::Stack::push(L, Polygon::decompose(shape, maxVertices));
    return 1;
}

int PolygonLua::area(lua_State* L) {
    lua::Stack::push(L, Polygon::getArea(readShape(L, 1)));
    return 1;
}

// Traces a field with `trace(values, width, height, {threshold = 0.5, spacing = 1, origin = {0, 0}})`, where `values` hold `width` times `height` numbers row by row.
int PolygonLua::trace(lua_State* L) {
    const auto values = lua::Stack::read<std::vector<float>>(L, 1);
    const auto width = lua::Stack::read<int>(L, 2);
    const auto height = lua::Stack::read<int>(L, 3);
    MarchingSquares::Options options;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kTraceFields});
        lua::Table::readField(L, 4, "threshold", options.threshold);
        lua::Table::readField(L, 4, "spacing", options.spacing);
        lua::Table::readField(L, 4, "origin", options.origin);
    }
    lua::Stack::push(L, MarchingSquares::trace(values, width, height, options));
    return 1;
}

// Traces the pixels that are `true` or not zero with `traceBitmap(pixels, width, height, {spacing = 1, origin = {0, 0}})`.
int PolygonLua::traceBitmap(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const auto width = lua::Stack::read<int>(L, 2);
    const auto height = lua::Stack::read<int>(L, 3);
    float spacing = 1.0F;
    Vec2 origin{};
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kBitmapFields});
        lua::Table::readField(L, 4, "spacing", spacing);
        lua::Table::readField(L, 4, "origin", origin);
    }

    const auto count = static_cast<std::size_t>(luaL_len(L, 1));
    std::vector<std::uint8_t> pixels(count);
    for (std::size_t pixel = 0; pixel < count; ++pixel) {
        const int type = lua_rawgeti(L, 1, static_cast<lua_Integer>(pixel + 1));
        pixels[pixel] = (type == LUA_TBOOLEAN ? lua_toboolean(L, -1) != 0 : lua_tonumber(L, -1) != 0.0) ? 1 : 0;
        lua_pop(L, 1);
    }
    lua::Stack::push(L, MarchingSquares::traceBitmap(pixels, width, height, spacing, origin));
    return 1;
}

void PolygonLua::addFunctions(lua_State* L) {
    const luaL_Reg polygon[] = {
        {"unite", &lua::Binding::native<&unite>}, {"subtract", &lua::Binding::native<&subtract>}, {"intersect", &lua::Binding::native<&intersect>}, {"exclude", &lua::Binding::native<&exclude>}, {"offset", &lua::Binding::native<&offset>}, {"simplify", &lua::Binding::native<&simplify>}, {"decompose", &lua::Binding::native<&decompose>}, {"area", &lua::Binding::native<&area>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, polygon);
    lua_setfield(L, -2, "polygon");

    const luaL_Reg marchingSquares[] = {
        {"trace", &lua::Binding::native<&trace>},
        {"traceBitmap", &lua::Binding::native<&traceBitmap>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, marchingSquares);
    lua_setfield(L, -2, "marchingSquares");
}

} // namespace haylen::math
