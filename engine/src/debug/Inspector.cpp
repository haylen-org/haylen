#include "debug/Inspector.hpp"

#include <lua.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <stdexcept>
#include <system_error>

#include <fast_float/fast_float.h>

#include "haylen/lua/Runtime.hpp"

namespace haylen::debug {

std::vector<std::string> Inspector::splitPath(std::string_view path) {
    std::vector<std::string> keys;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = std::min(path.find('.', start), path.size());
        if (end > start) {
            keys.emplace_back(path.substr(start, end - start));
        }
        start = end + 1;
    }
    return keys;
}

std::vector<Inspector::Entry> Inspector::list(lua_State* L, std::span<const std::string> path) {
    std::vector<Entry> entries;
    if (path.empty()) {
        // clang-format off
        lua::Runtime::protectedRun(L, [&entries](lua_State* state) {
            for (const std::string_view root : kRoots) {
                pushRoot(state, root);
                entries.push_back(describe(state, std::string(root), lua_gettop(state), false));
                lua_pop(state, 1);
            }
        });
        // clang-format on
        return entries;
    }

    lua::Runtime::protectedRun(L, [&entries, path](lua_State* state) { collect(state, path, entries); });

    // Integer keys come first in their order, and names follow in the order of the alphabet.
    // clang-format off
    std::ranges::sort(entries, [](const Entry& left, const Entry& right) {
        const std::optional<std::int64_t> first = readIndex(left.key);
        const std::optional<std::int64_t> second = readIndex(right.key);
        if (first.has_value() != second.has_value()) {
            return first.has_value();
        }
        return first ? *first < *second : left.key < right.key;
    });
    // clang-format on
    return entries;
}

// The body of a protected run has a stack of its own, so the entries leave it as C++ values.
void Inspector::collect(lua_State* L, std::span<const std::string> path, std::vector<Entry>& entries) {
    pushPath(L, path);
    const int value = lua_gettop(L);
    if (lua_type(L, value) == LUA_TTABLE) {
        lua_pushnil(L);
        while (lua_next(L, value) != 0 && entries.size() < kMaxEntries) {
            entries.push_back(describe(L, describeKey(L, -2), lua_gettop(L), true));
            lua_pop(L, 1);
        }
        return;
    }
    if (lua_type(L, value) != LUA_TUSERDATA || lua_getmetatable(L, value) == 0 || lua_getfield(L, -1, "__properties") != LUA_TTABLE) {
        throw std::invalid_argument(std::format("The value at \"{}\" is of the type \"{}\", which holds nothing to list.", joinPath(path), luaL_typename(L, value)));
    }
    const int getters = lua_gettop(L);
    lua_getfield(L, getters - 1, "__writable");
    const int setters = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, getters) != 0 && entries.size() < kMaxEntries) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        const bool writable = lua_istable(L, setters) && lua_rawget(L, setters) != LUA_TNIL;
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_gettable(L, value);
        entries.push_back(describe(L, describeKey(L, -2), lua_gettop(L), writable));
        lua_pop(L, 1);
    }
}

void Inspector::set(lua_State* L, std::span<const std::string> path, std::string_view text) {
    if (path.size() < 2) {
        throw std::invalid_argument("The inspector edits values inside a root, such as \"watched.player.speed\".");
    }
    lua::Runtime::protectedRun(L, [path, text](lua_State* state) { assign(state, path, text); });
}

void Inspector::assign(lua_State* L, std::span<const std::string> path, std::string_view text) {
    pushPath(L, path.first(path.size() - 1));
    const int container = lua_gettop(L);
    pushKey(L, container, path.back());
    const int key = lua_gettop(L);
    lua_pushvalue(L, key);
    lua_gettable(L, container);
    const int current = lua_type(L, -1);
    lua_pop(L, 1);

    lua_pushvalue(L, key);
    if (current == LUA_TNUMBER) {
        double number = 0.0;
        const auto [end, error] = fast_float::from_chars(text.data(), text.data() + text.size(), number);
        if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(number)) {
            throw std::invalid_argument(std::format("The value at \"{}\" is a number, and \"{}\" is not one.", joinPath(path), text));
        }
        if (std::trunc(number) == number && std::abs(number) < 9.0e15) {
            lua_pushinteger(L, static_cast<lua_Integer>(number));
        } else {
            lua_pushnumber(L, number);
        }
    } else if (current == LUA_TBOOLEAN) {
        if (text != "true" && text != "false") {
            throw std::invalid_argument(std::format("The value at \"{}\" is a boolean, which is \"true\" or \"false\".", joinPath(path)));
        }
        lua_pushboolean(L, text == "true" ? 1 : 0);
    } else if (current == LUA_TSTRING) {
        lua_pushlstring(L, text.data(), text.size());
    } else {
        throw std::invalid_argument(std::format("The value at \"{}\" is of the type \"{}\", which the inspector does not edit. It edits numbers, booleans and strings.", joinPath(path), lua_typename(L, current)));
    }
    lua_settable(L, container);
}

void Inspector::watch(lua_State* L, std::string_view name, int index) {
    const int value = lua_absindex(L, index);
    pushWatched(L);
    lua_pushlstring(L, name.data(), name.size());
    lua_pushvalue(L, value);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

// The table of watched values holds them weakly, so watching never keeps an owner alive.
void Inspector::pushWatched(lua_State* L) {
    if (lua_getfield(L, LUA_REGISTRYINDEX, kWatchedKey) == LUA_TTABLE) {
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_createtable(L, 0, 1);
    lua_pushliteral(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, kWatchedKey);
}

void Inspector::pushRoot(lua_State* L, std::string_view name) {
    if (name == "watched") {
        pushWatched(L);
    } else if (name == "scenes") {
        pushModuleField(L, "haylen.scene", "list", true);
    } else if (name == "autoloads") {
        pushModuleField(L, "haylen", "autoloads", false);
    } else if (name == "globals") {
        lua_pushglobaltable(L);
    } else {
        throw std::invalid_argument(std::format("The inspector has no root \"{}\". Its roots are \"watched\", \"scenes\", \"autoloads\" and \"globals\".", name));
    }
}

void Inspector::pushModuleField(lua_State* L, const char* module, const char* field, bool call) {
    lua_getglobal(L, "require");
    lua_pushstring(L, module);
    lua_call(L, 1, 1);
    lua_getfield(L, -1, field);
    lua_remove(L, -2);
    if (call) {
        lua_call(L, 0, 1);
    }
}

void Inspector::pushPath(lua_State* L, std::span<const std::string> path) {
    pushRoot(L, path.front());
    for (std::size_t index = 1; index < path.size(); ++index) {
        const int container = lua_gettop(L);
        if (!lua_istable(L, container) && !lua_isuserdata(L, container)) {
            throw std::invalid_argument(std::format("The value at \"{}\" is of the type \"{}\", which holds no \"{}\".", joinPath(path.first(index)), luaL_typename(L, container), path[index]));
        }
        pushKey(L, container, path[index]);
        lua_gettable(L, container);
        lua_remove(L, container);
    }
}

// A key that reads as an integer names the item of a sequence when the table holds no text key of that name.
void Inspector::pushKey(lua_State* L, int container, const std::string& key) {
    const std::optional<std::int64_t> number = readIndex(key);
    if (number && lua_istable(L, container)) {
        lua_pushlstring(L, key.data(), key.size());
        if (lua_rawget(L, container) == LUA_TNIL) {
            lua_pop(L, 1);
            lua_pushinteger(L, *number);
            return;
        }
        lua_pop(L, 1);
    }
    lua_pushlstring(L, key.data(), key.size());
}

// Only a key that is a whole number throughout names an integer key, so a name such as `2x` stays a name.
std::optional<std::int64_t> Inspector::readIndex(std::string_view key) noexcept {
    std::int64_t number = 0;
    const char* end = key.data() + key.size();
    const std::from_chars_result read = std::from_chars(key.data(), end, number);
    if (read.ec != std::errc{} || read.ptr != end) {
        return std::nullopt;
    }
    return number;
}

Inspector::Entry Inspector::describe(lua_State* L, std::string key, int index, bool editable) {
    const int type = lua_type(L, index);
    std::string name = lua_typename(L, type);
    if (type == LUA_TUSERDATA && lua_getmetatable(L, index) != 0) {
        if (lua_getfield(L, -1, "__name") == LUA_TSTRING) {
            name = lua_tostring(L, -1);
        }
        lua_pop(L, 2);
    }
    const bool primitive = type == LUA_TNUMBER || type == LUA_TBOOLEAN || type == LUA_TSTRING;
    return {.key = std::move(key), .type = std::move(name), .value = describeValue(L, index), .editable = editable && primitive, .expandable = type == LUA_TTABLE || type == LUA_TUSERDATA};
}

std::string Inspector::describeValue(lua_State* L, int index) {
    std::string text;
    switch (lua_type(L, index)) {
    case LUA_TNUMBER:
        text = lua_isinteger(L, index) != 0 ? std::to_string(lua_tointeger(L, index)) : std::format("{}", lua_tonumber(L, index));
        break;
    case LUA_TBOOLEAN:
        text = lua_toboolean(L, index) != 0 ? "true" : "false";
        break;
    case LUA_TSTRING: {
        std::size_t length = 0;
        const char* characters = lua_tolstring(L, index, &length);
        text.assign(characters, std::min(length, kMaxValueText));
        break;
    }
    case LUA_TTABLE:
        text = describeTable(L, index);
        break;
    case LUA_TNIL:
        text = "nil";
        break;
    default:
        break;
    }
    return text;
}

// Counts the values of a table up to the most that a listing shows, since a table that is not a sequence has no length.
std::string Inspector::describeTable(lua_State* L, int index) {
    const int table = lua_absindex(L, index);
    std::size_t count = 0;
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        lua_pop(L, 1);
        if (++count > kMaxEntries) {
            lua_pop(L, 1);
            return std::format("More than {} values", kMaxEntries);
        }
    }
    if (count == 0) {
        return "Empty";
    }
    const std::string values = count == 1 ? "1 value" : std::format("{} values", count);

    // A table with a name, such as a scene or an object of the app, shows it before its count.
    lua_pushliteral(L, "name");
    if (lua_rawget(L, table) != LUA_TSTRING) {
        lua_pop(L, 1);
        return values;
    }
    std::size_t length = 0;
    const char* characters = lua_tolstring(L, -1, &length);
    std::string named = std::format("{}, {}", std::string_view(characters, std::min(length, kMaxValueText)), values);
    lua_pop(L, 1);
    return named;
}

// Keys are read without converting them in place, which would break the traversal of the table.
std::string Inspector::describeKey(lua_State* L, int index) {
    switch (lua_type(L, index)) {
    case LUA_TSTRING:
        return lua_tostring(L, index);
    case LUA_TNUMBER:
        return lua_isinteger(L, index) != 0 ? std::to_string(lua_tointeger(L, index)) : std::format("{}", lua_tonumber(L, index));
    default:
        return std::format("({})", luaL_typename(L, index));
    }
}

std::string Inspector::joinPath(std::span<const std::string> path) {
    std::string joined;
    for (const std::string& key : path) {
        joined += joined.empty() ? key : "." + key;
    }
    return joined;
}

} // namespace haylen::debug
