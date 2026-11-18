#include "core/FloatBufferLua.hpp"

#include <lua.hpp>

#include <cstdint>
#include <memory>
#include <span>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Converter.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::core {

FloatBuffer& FloatBufferLua::check(lua_State* L) {
    return lua::Userdata::check<FloatBuffer>(L, 1);
}

// The range is checked in 64 bits before any number narrows to a size, which holds only 32 bits on the web.
std::size_t FloatBufferLua::checkPosition(lua_State* L, const FloatBuffer& buffer, int index, std::uint64_t count) {
    int isInteger = 0;
    const lua_Integer position = lua_tointegerx(L, index, &isInteger);
    if (isInteger == 0 || position < 1 || static_cast<std::uint64_t>(position - 1) + count > buffer.size()) {
        // The last position follows the integer arithmetic of Lua, which wraps around, so a range that starts near the largest integer never overflows.
        const auto last = static_cast<lua_Integer>(static_cast<lua_Unsigned>(position) + count - 1);
        luaL_error(L, "Float buffer positions %I to %I fall outside its size of %I.", position, last, static_cast<lua_Integer>(buffer.size()));
    }

    return static_cast<std::size_t>(position - 1);
}

// Reads `buffer[index]`, which never goes through a method lookup.
int FloatBufferLua::at(lua_State* L) {
    const FloatBuffer& buffer = check(L);
    const std::size_t position = checkPosition(L, buffer, 2, 1);
    lua_pushnumber(L, static_cast<lua_Number>(buffer.getValues()[position]));
    return 1;
}

int FloatBufferLua::assign(lua_State* L) {
    FloatBuffer& buffer = check(L);
    const std::size_t position = checkPosition(L, buffer, 2, 1);
    buffer.getValues()[position] = lua::Stack::read<float>(L, 3);
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
    const std::uint64_t requested = list ? lua_rawlen(L, 3) : static_cast<std::uint64_t>(lua_gettop(L) - 2);
    const std::size_t first = checkPosition(L, buffer, 2, requested);
    const auto count = static_cast<std::size_t>(requested);
    const std::span<float> values = buffer.getValues().subspan(first, count);
    for (std::size_t index = 0; index < count; ++index) {
        if (!list) {
            values[index] = lua::Stack::read<float>(L, static_cast<int>(index) + 3);
            continue;
        }

        int isNumber = 0;
        lua_rawgeti(L, 3, static_cast<lua_Integer>(index + 1));
        const lua_Number value = lua_tonumberx(L, -1, &isNumber);
        lua_pop(L, 1);
        if (isNumber == 0) {
            return luaL_error(L, "Float buffer values are numbers, and entry %I of the list is not.", static_cast<lua_Integer>(index + 1));
        }

        values[index] = lua::Converter<float>::fromNumber(L, value);
    }

    return 0;
}

// Returns `count` values from a position with `get(first[, count])`, one value by default.
int FloatBufferLua::get(lua_State* L) {
    const FloatBuffer& buffer = check(L);
    const lua_Integer requested = luaL_optinteger(L, 3, 1);
    luaL_argcheck(L, requested >= 0, 3, "the count cannot be negative");
    const std::size_t first = checkPosition(L, buffer, 2, static_cast<std::uint64_t>(requested));
    const auto count = static_cast<std::size_t>(requested);
    const std::span<const float> values = buffer.getValues().subspan(first, count);
    luaL_checkstack(L, static_cast<int>(count), "too many float buffer values to return at once");
    for (const float value : values) {
        lua_pushnumber(L, static_cast<lua_Number>(value));
    }

    return static_cast<int>(count);
}

// Sets every value with `fill(value)`, or `count` values from a position with `fill(value, first, count)`.
int FloatBufferLua::fill(lua_State* L) {
    FloatBuffer& buffer = check(L);
    const auto value = lua::Stack::read<float>(L, 2);
    if (lua_isnoneornil(L, 3)) {
        buffer.fill(value);
        return 0;
    }

    const lua_Integer requested = luaL_checkinteger(L, 4);
    luaL_argcheck(L, requested >= 0, 4, "the count cannot be negative");
    const std::size_t first = checkPosition(L, buffer, 3, static_cast<std::uint64_t>(requested));
    const auto count = static_cast<std::size_t>(requested);
    for (float& target : buffer.getValues().subspan(first, count)) {
        target = value;
    }

    return 0;
}

int FloatBufferLua::create(lua_State* L) {
    const lua_Integer size = luaL_checkinteger(L, 1);
    luaL_argcheck(L, size >= 0, 1, "the size cannot be negative");
    if (static_cast<std::uint64_t>(size) > FloatBuffer::kMaxSize) {
        return luaL_error(L, "A float buffer holds at most %d floats.", static_cast<int>(FloatBuffer::kMaxSize));
    }
    lua::Userdata::emplace<FloatBuffer>(L, std::make_shared<FloatBuffer>(static_cast<std::size_t>(size), lua::Converter<float>::readOr(L, 2, 0.0)));
    return 1;
}

void FloatBufferLua::install(lua_State* L) {
    lua::ClassBuilder<FloatBuffer>(L).function("set", &lua::Binding::native<&set>).function("get", &lua::Binding::native<&get>).function("fill", &lua::Binding::native<&fill>).meta("__len", &length).indexer(&at, &assign).install();
}

} // namespace haylen::core
