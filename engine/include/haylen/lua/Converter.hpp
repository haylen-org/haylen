#pragma once

#include <lua.hpp>

#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

// Converts values between C++ and the Lua stack. Specializations exist for scalars, strings, enums with names, math types and bound userdata.
template <typename T> struct Converter;

template <ValueBound T> struct Converter<T> {
    static void push(lua_State* L, T value) {
        Userdata::emplace<T>(L, std::move(value));
    }
    static T read(lua_State* L, int index) {
        return Userdata::check<T>(L, index);
    }
    static bool is(lua_State* L, int index) {
        return Userdata::test<T>(L, index) != nullptr;
    }
};

template <SharedBound T> struct Converter<std::shared_ptr<T>> {
    static void push(lua_State* L, std::shared_ptr<T> value) {
        if (!value) {
            lua_pushnil(L);
            return;
        }
        Userdata::emplace<T>(L, std::move(value));
    }
    static std::shared_ptr<T> read(lua_State* L, int index) {
        return Userdata::checkShared<T>(L, index);
    }
    static bool is(lua_State* L, int index) {
        return Userdata::test<T>(L, index) != nullptr;
    }
};

template <> struct Converter<bool> {
    static void push(lua_State* L, bool value) {
        lua_pushboolean(L, value ? 1 : 0);
    }
    static bool read(lua_State* L, int index) {
        luaL_checktype(L, index, LUA_TBOOLEAN);
        return lua_toboolean(L, index) != 0;
    }
    static bool is(lua_State* L, int index) {
        return lua_isboolean(L, index);
    }
};

template <typename T>
concept Integer = std::integral<T> && !std::same_as<T, bool>;

template <Integer T> struct Converter<T> {
    static void push(lua_State* L, T value) {
        lua_pushinteger(L, static_cast<lua_Integer>(value));
    }
    static T read(lua_State* L, int index) {
        const lua_Integer value = luaL_checkinteger(L, index);
        if constexpr (std::is_unsigned_v<T>) {
            luaL_argcheck(L, value >= 0, index, "expected a non-negative integer");
        }
        // Narrow integers never wrap silently, so a light mask of 300 is an error instead of 44.
        luaL_argcheck(L, std::in_range<T>(value), index, "integer out of range");
        return static_cast<T>(value);
    }
    static bool is(lua_State* L, int index) {
        return lua_isinteger(L, index) != 0;
    }
};

// A number reaches Lua without a negative zero, which adding zero turns positive, so the coordinates of a rectangle at the origin never print as "-0". A finite number of Lua past the range of the type is an error, since converting it is undefined, while infinities and NaN convert as they are.
template <std::floating_point T> struct Converter<T> {
    static void push(lua_State* L, T value) {
        lua_pushnumber(L, static_cast<lua_Number>(value) + 0.0);
    }
    static T read(lua_State* L, int index) {
        const lua_Number value = luaL_checknumber(L, index);
        luaL_argcheck(L, fits(value), index, "number out of range");
        return static_cast<T>(value);
    }
    static bool is(lua_State* L, int index) {
        return lua_type(L, index) == LUA_TNUMBER;
    }

    // Reads an optional argument, which is the fallback when it is absent or nil.
    static T readOr(lua_State* L, int index, lua_Number fallback) {
        const lua_Number value = luaL_optnumber(L, index, fallback);
        luaL_argcheck(L, fits(value), index, "number out of range");
        return static_cast<T>(value);
    }

    // Converts a number that is no argument, such as a field of a table. Only a float holds fewer numbers than Lua, so the error names the range of a float.
    static T fromNumber(lua_State* L, lua_Number value) {
        if (!fits(value)) {
            luaL_error(L, "The number %f lies outside the numbers from -3.4028234663853e+38 to 3.4028234663853e+38 that the engine holds.", value);
        }
        return static_cast<T>(value);
    }

    [[nodiscard]] static bool fits(lua_Number value) noexcept {
        return !std::isfinite(value) || std::fabs(value) <= static_cast<lua_Number>(std::numeric_limits<T>::max());
    }
};

template <> struct Converter<std::string> {
    static void push(lua_State* L, const std::string& value) {
        lua_pushlstring(L, value.data(), value.size());
    }
    static std::string read(lua_State* L, int index) {
        std::size_t length = 0;
        const char* text = luaL_checklstring(L, index, &length);
        return {text, length};
    }
    static bool is(lua_State* L, int index) {
        return lua_type(L, index) == LUA_TSTRING;
    }
};

template <> struct Converter<std::string_view> {
    static void push(lua_State* L, std::string_view value) {
        lua_pushlstring(L, value.data(), value.size());
    }
    static std::string_view read(lua_State* L, int index) {
        std::size_t length = 0;
        const char* text = luaL_checklstring(L, index, &length);
        return {text, length};
    }
    static bool is(lua_State* L, int index) {
        return lua_type(L, index) == LUA_TSTRING;
    }
};

template <> struct Converter<const char*> {
    static void push(lua_State* L, const char* value) {
        lua_pushstring(L, value);
    }
};

template <typename T> struct Converter<std::optional<T>> {
    static void push(lua_State* L, const std::optional<T>& value) {
        if (value) {
            Converter<T>::push(L, *value);
            return;
        }
        lua_pushnil(L);
    }
    static std::optional<T> read(lua_State* L, int index) {
        if (lua_isnoneornil(L, index)) {
            return std::nullopt;
        }
        return Converter<T>::read(L, index);
    }
    static bool is(lua_State* L, int index) {
        return lua_isnoneornil(L, index) || Converter<T>::is(L, index);
    }
};

template <typename T> struct Converter<std::vector<T>> {
    static void push(lua_State* L, const std::vector<T>& values) {
        lua_createtable(L, static_cast<int>(values.size()), 0);
        for (std::size_t index = 0; index < values.size(); ++index) {
            Converter<T>::push(L, values[index]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }
    }
    // The elements are read raw, so the length is the raw one too, which no `__len` of the table can make up.
    static std::vector<T> read(lua_State* L, int index) {
        luaL_checktype(L, index, LUA_TTABLE);
        const int table = lua_absindex(L, index);
        const auto length = static_cast<std::size_t>(lua_rawlen(L, table));
        std::vector<T> values;
        values.reserve(length);
        for (std::size_t element = 1; element <= length; ++element) {
            lua_rawgeti(L, table, static_cast<lua_Integer>(element));
            values.push_back(Converter<T>::read(L, -1));
            lua_pop(L, 1);
        }
        return values;
    }
    static bool is(lua_State* L, int index) {
        return lua_istable(L, index);
    }
};

template <NamedEnum T> struct Converter<T> {
    static void push(lua_State* L, T value) {
        Converter<std::string_view>::push(L, EnumNames<T>::name(value));
    }
    static T read(lua_State* L, int index) {
        const std::string_view name = Converter<std::string_view>::read(L, index);
        const std::optional<T> value = EnumNames<T>::fromName(name);
        if (!value) {
            luaL_argerror(L, index, lua_pushfstring(L, "unknown value '%s'", std::string(name).c_str()));
        }
        return *value;
    }
    static bool is(lua_State* L, int index) {
        return lua_type(L, index) == LUA_TSTRING && EnumNames<T>::fromName(Converter<std::string_view>::read(L, index)).has_value();
    }
};

} // namespace haylen::lua
