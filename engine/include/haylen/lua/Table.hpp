#pragma once

#include <lua.hpp>

#include <concepts>
#include <exception>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "haylen/lua/Converter.hpp"

namespace haylen::lua {

// Reads option tables, whose fields are optional and whose unknown keys are errors.
class Table final {
  public:
    using FieldNames = std::span<const std::string_view>;

    // Raises a Lua error for the first key of the table at index that none of the lists name, so a misspelled option never goes unnoticed.
    static void checkFields(lua_State* L, int index, std::span<const FieldNames> allowed);

    static void checkFields(lua_State* L, int index, std::initializer_list<FieldNames> allowed) {
        checkFields(L, index, std::span<const FieldNames>(allowed.begin(), allowed.size()));
    }

    // Reads an optional field of the table at index into `target`, leaving `target` untouched when the field is absent.
    template <typename T> static void readField(lua_State* L, int index, const char* field, T& target) {
        lua_getfield(L, index, field);
        if (!lua_isnil(L, -1)) {
            readValue(L, field, target);
        }
        lua_pop(L, 1);
    }

    // Reads the option table at index in one pass over the keys it holds, which suits options read for every draw: `read` receives each key that `own` names with its value on top of the stack, the keys that `extras` name belong to another reader of the same table, and any other key raises the unknown option error.
    template <typename Read> static void readFields(lua_State* L, int index, FieldNames own, std::initializer_list<FieldNames> extras, Read&& read) {
        const int table = lua_absindex(L, index);
        lua_pushnil(L);
        while (lua_next(L, table) != 0) {
            if (const std::optional<std::string_view> key = findKey(L, own, extras)) {
                read(*key);
            }
            lua_pop(L, 1);
        }
    }

    // Reads the value on top of the stack into `target` as the option named `field`, and leaves it there.
    // Converters report problems as argument errors, so a value that is not a plain number or boolean is read in a protected call, and a failure names the option instead.
    template <typename T> static void readValue(lua_State* L, std::string_view field, T& target) {
        if constexpr (std::floating_point<T>) {
            if (lua_type(L, -1) == LUA_TNUMBER) {
                target = static_cast<T>(lua_tonumber(L, -1));
                return;
            }
        } else if constexpr (std::same_as<T, bool>) {
            if (lua_isboolean(L, -1)) {
                target = lua_toboolean(L, -1) != 0;
                return;
            }
        }
        lua_pushcfunction(L, &readProtected<T>);
        lua_pushvalue(L, -2);
        lua_pushlightuserdata(L, &target);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            raiseFieldError(L, field);
        }
    }

  private:
    template <typename T> static int readProtected(lua_State* L) {
        std::string message;
        try {
            *static_cast<T*>(lua_touserdata(L, 2)) = Converter<T>::read(L, 1);
            return 0;
        } catch (const std::exception& exception) {
            message = exception.what();
        }
        return luaL_error(L, "%s", message.c_str());
    }

    // Returns the key under the value on top of the stack when `own` names it, nothing when `extras` names it, and raises an error for any other key.
    static std::optional<std::string_view> findKey(lua_State* L, FieldNames own, std::initializer_list<FieldNames> extras);

    // Raises the error at the top of the stack again as a problem with the option named `field` of the running function.
    static int raiseFieldError(lua_State* L, std::string_view field);
};

} // namespace haylen::lua
