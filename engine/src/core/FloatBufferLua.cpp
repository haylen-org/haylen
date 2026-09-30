#include "core/FloatBufferLua.hpp"

#include <lua.hpp>

#include <span>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::core {

FloatBuffer& FloatBufferLua::check(lua_State* L) {
    return lua::Userdata::check<FloatBuffer>(L, 1);
}

std::size_t FloatBufferLua::checkPosition(lua_State* L, const FloatBuffer& buffer, int index, std::size_t count) {
    int isInteger = 0;
    const lua_Integer position = lua_tointegerx(L, index, &isInteger);
    if (isInteger == 0 || position < 1 || static_cast<std::size_t>(position - 1) + count > buffer.size()) {
        luaL_error(L, "Float buffer positions %I to %I fall outside its size of %I.", position, position + static_cast<lua_Integer>(count) - 1, static_cast<lua_Integer>(buffer.size()));
    }
    return static_cast<std::size_t>(position - 1);
}

// Reads `buffer[index]`, which never goes through a method lookup.
int FloatBufferLua::at(lua_State* L) {
    const FloatBuffer& buffer = check(L);
    lua_pushnumber(L, static_cast<lua_Number>(buffer.getValues()[checkPosition(L, buffer, 2, 1)]));
    return 1;
}

int FloatBufferLua::assign(lua_State* L) {
    FloatBuffer& buffer = check(L);
    const std::size_t position = checkPosition(L, buffer, 2, 1);
    buffer.getValues()[position] = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

int FloatBufferLua::length(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(check(L).size()));
    return 1;
}

// Copies numbers in from a position with `set(first, ...)` or from an array with `set(first, list)`.
int FloatBufferLua::set(lua_State* L) {
    FloatBuffer& buffer = check(L);
    const bool list = lua_istable(L, 3);
    const auto count = list ? static_cast<std::size_t>(lua_rawlen(L, 3)) : static_cast<std::size_t>(lua_gettop(L) - 2);
    const std::span<float> values = buffer.getValues().subspan(checkPosition(L, buffer, 2, count), count);
    for (std::size_t index = 0; index < count; ++index) {
        if (!list) {
            values[index] = static_cast<float>(luaL_checknumber(L, static_cast<int>(index) + 3));
            continue;
        }
        int isNumber = 0;
        lua_rawgeti(L, 3, static_cast<lua_Integer>(index + 1));
        const lua_Number value = lua_tonumberx(L, -1, &isNumber);
        lua_pop(L, 1);
        if (isNumber == 0) {
            return luaL_error(L, "Float buffer values are numbers, and entry %I of the list is not.", static_cast<lua_Integer>(index + 1));
        }
        values[index] = static_cast<float>(value);
    }
    return 0;
}

// Returns `count` values from a position with `get(first[, count])`, one value by default.
int FloatBufferLua::get(lua_State* L) {
    const FloatBuffer& buffer = check(L);
    const lua_Integer requested = luaL_optinteger(L, 3, 1);
    luaL_argcheck(L, requested >= 0, 3, "the count cannot be negative");
    const auto count = static_cast<std::size_t>(requested);
    const std::span<const float> values = buffer.getValues().subspan(checkPosition(L, buffer, 2, count), count);
    luaL_checkstack(L, static_cast<int>(count), "too many float buffer values to return at once");
    for (const float value : values) {
        lua_pushnumber(L, static_cast<lua_Number>(value));
    }
    return static_cast<int>(count);
}

// Sets every value with `fill(value)`, or `count` values from a position with `fill(value, first, count)`.
int FloatBufferLua::fill(lua_State* L) {
    FloatBuffer& buffer = check(L);
    const auto value = static_cast<float>(luaL_checknumber(L, 2));
    if (lua_isnoneornil(L, 3)) {
        buffer.fill(value);
        return 0;
    }
    const lua_Integer requested = luaL_checkinteger(L, 4);
    luaL_argcheck(L, requested >= 0, 4, "the count cannot be negative");
    const auto count = static_cast<std::size_t>(requested);
    for (float& target : buffer.getValues().subspan(checkPosition(L, buffer, 3, count), count)) {
        target = value;
    }
    return 0;
}

int FloatBufferLua::create(lua_State* L) {
    const lua_Integer size = luaL_checkinteger(L, 1);
    luaL_argcheck(L, size >= 0, 1, "the size cannot be negative");
    lua::Userdata::emplace<FloatBuffer>(L, std::make_shared<FloatBuffer>(static_cast<std::size_t>(size), static_cast<float>(luaL_optnumber(L, 2, 0.0))));
    return 1;
}

void FloatBufferLua::install(lua_State* L) {
    lua::ClassBuilder<FloatBuffer>(L).function("set", &lua::Binding::native<&set>).function("get", &lua::Binding::native<&get>).function("fill", &lua::Binding::native<&fill>).meta("__len", &length).indexer(&at, &assign).install();
}

} // namespace haylen::core
