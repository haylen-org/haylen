#pragma once

#include <lua.hpp>

#include <exception>
#include <initializer_list>
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

    // Reads an optional field of the table at index into target, leaving target untouched when the field is absent.
    // Converters report problems as argument errors, so the value is read in a protected call and a failure names the option instead.
    template <typename T> static void readField(lua_State* L, int index, const char* field, T& target) {
        lua_getfield(L, index, field);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return;
        }
        lua_pushcfunction(L, &readProtected<T>);
        lua_insert(L, -2);
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

    // Raises the error at the top of the stack again as a problem with the option named field of the running function.
    static int raiseFieldError(lua_State* L, const char* field);
};

} // namespace haylen::lua
