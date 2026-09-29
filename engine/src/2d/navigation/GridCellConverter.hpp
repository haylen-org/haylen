#pragma once

#include <lua.hpp>

#include "haylen/2d/navigation/Grid.hpp"
#include "haylen/lua/Converter.hpp"

namespace haylen::lua {

// Cells cross into Lua as {x = column, y = row} tables and come back from those or from {column, row}.
template <> struct Converter<navigation2d::Grid::Cell> {
    static void push(lua_State* L, navigation2d::Grid::Cell cell) {
        lua_createtable(L, 0, 2);
        lua_pushinteger(L, cell.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, cell.y);
        lua_setfield(L, -2, "y");
    }

    static navigation2d::Grid::Cell read(lua_State* L, int index) {
        luaL_checktype(L, index, LUA_TTABLE);
        const int table = lua_absindex(L, index);
        const bool named = lua_getfield(L, table, "x") != LUA_TNIL;
        lua_pop(L, 1);
        if (named) {
            lua_getfield(L, table, "x");
            lua_getfield(L, table, "y");
        } else {
            lua_rawgeti(L, table, 1);
            lua_rawgeti(L, table, 2);
        }
        const navigation2d::Grid::Cell cell{static_cast<int>(luaL_checkinteger(L, -2)), static_cast<int>(luaL_checkinteger(L, -1))};
        lua_pop(L, 2);
        return cell;
    }
};

} // namespace haylen::lua
