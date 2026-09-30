#include "haylen/lua/Table.hpp"

#include <algorithm>

namespace haylen::lua {

void Table::checkFields(lua_State* L, int index, std::span<const FieldNames> allowed) {
    const int table = lua_absindex(L, index);
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        lua_pop(L, 1);
        if (lua_type(L, -1) != LUA_TSTRING) {
            luaL_error(L, "Option tables only accept string keys.");
        }

        const std::string_view key = lua_tostring(L, -1);
        const bool known = std::any_of(allowed.begin(), allowed.end(), [key](FieldNames names) { return std::find(names.begin(), names.end(), key) != names.end(); });
        if (!known) {
            luaL_error(L, "Unknown option \"%s\".", std::string(key).c_str());
        }
    }
}

int Table::raiseFieldError(lua_State* L, const char* field) {
    // The protected reader failed as an anonymous function, so only the reason in the parentheses of its argument error is kept. A reason that is a sentence of its own keeps its period.
    std::string reason = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "its value cannot be read";
    if (const std::size_t open = reason.find(" ("); reason.starts_with("bad argument") && open != std::string::npos && reason.ends_with(')')) {
        reason = reason.substr(open + 2, reason.size() - open - 3);
    }
    if (!reason.ends_with('.')) {
        reason += '.';
    }
    lua_pop(L, 1);

    lua_Debug frame{};
    const char* name = lua_getstack(L, 0, &frame) != 0 && lua_getinfo(L, "n", &frame) != 0 && frame.name != nullptr ? frame.name : "?";
    return luaL_error(L, "The option \"%s\" of \"%s\" is invalid: %s", field, name, reason.c_str());
}

} // namespace haylen::lua
