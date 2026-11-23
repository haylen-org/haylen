#include "debug/ScriptInspector.hpp"

#include <lua.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>

#include <fast_float/fast_float.h>

#include "haylen/core/Utf8.hpp"
#include "haylen/lua/Runtime.hpp"

namespace haylen::debug {

ScriptInspector::ScriptInspector(lua_State* L) noexcept : state(L) {}

// The entries fill the storage of the listing before, so a listing of the same values allocates nothing.
void ScriptInspector::list(std::span<const std::string> path, Inspector::Listing& listing) {
    lua_State* L = state;
    std::size_t count = 0;
    std::vector<Inspector::Entry>& found = listing.found;
    if (path.empty()) {
        // clang-format off
        lua::Runtime::protectedRun(L, [&found, &count](lua_State* thread) {
            for (const std::string_view root : kRoots) {
                pushRoot(thread, root);
                Inspector::Entry& entry = takeEntry(found, count);
                entry.key = root;
                describe(thread, lua_gettop(thread), false, entry);
                lua_pop(thread, 1);
            }
        });
        // clang-format on
    } else {
        lua::Runtime::protectedRun(L, [&found, &count, path](lua_State* thread) { collect(thread, path, found, count); });
    }

    found.resize(count);
    listing.order.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        listing.order[index] = index;
    }

    if (!path.empty()) {
        sort(listing);
    }

    listing.entries.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        listing.entries[index] = found[listing.order[index]];
    }
}

// Integer keys come first in their order, and names follow in the order of the alphabet. The places sort, so every entry of the listing takes the same values each time.
void ScriptInspector::sort(Inspector::Listing& listing) {
    const std::vector<Inspector::Entry>& found = listing.found;
    // clang-format off
    std::ranges::sort(listing.order, [&found](std::size_t left, std::size_t right) {
        const std::optional<std::int64_t> first = readIndex(found[left].key);
        const std::optional<std::int64_t> second = readIndex(found[right].key);
        if (first.has_value() != second.has_value()) {
            return first.has_value();
        }

        return first ? *first < *second : found[left].key < found[right].key;
    });
    // clang-format on
}

Inspector::Entry& ScriptInspector::takeEntry(std::vector<Inspector::Entry>& found, std::size_t& count) {
    if (count == found.size()) {
        found.emplace_back();
    }

    return found[count++];
}

// The body of a protected run has a stack of its own, so the entries leave it as C++ values.
void ScriptInspector::collect(lua_State* L, std::span<const std::string> path, std::vector<Inspector::Entry>& found, std::size_t& count) {
    pushPath(L, path);
    const int value = lua_gettop(L);
    if (lua_type(L, value) == LUA_TTABLE) {
        lua_pushnil(L);
        while (lua_next(L, value) != 0 && count < Inspector::kMaxEntries) {
            Inspector::Entry& entry = takeEntry(found, count);
            describeKey(L, -2, entry.key);
            describe(L, lua_gettop(L), true, entry);
            lua_pop(L, 1);
        }

        return;
    }

    if (lua_type(L, value) != LUA_TUSERDATA || lua_getmetatable(L, value) == 0 || lua_getfield(L, -1, "__properties") != LUA_TTABLE) {
        throw std::invalid_argument(std::format("The value at \"{}\" is of the type \"{}\", which holds nothing to list.", Inspector::joinPath(path), luaL_typename(L, value)));
    }

    const int getters = lua_gettop(L);
    lua_getfield(L, getters - 1, "__writable");
    const int setters = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, getters) != 0 && count < Inspector::kMaxEntries) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        const bool writable = lua_istable(L, setters) && lua_rawget(L, setters) != LUA_TNIL;
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_gettable(L, value);
        Inspector::Entry& entry = takeEntry(found, count);
        describeKey(L, -2, entry.key);
        describe(L, lua_gettop(L), writable, entry);
        lua_pop(L, 1);
    }
}

void ScriptInspector::set(std::span<const std::string> path, std::string_view text) {
    lua_State* L = state;
    if (path.size() < 2) {
        throw std::invalid_argument("The inspector edits values inside a root, such as \"watched.player.speed\".");
    }

    lua::Runtime::protectedRun(L, [path, text](lua_State* thread) { assign(thread, path, text); });
}

void ScriptInspector::assign(lua_State* L, std::span<const std::string> path, std::string_view text) {
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
            throw std::invalid_argument(std::format("The value at \"{}\" is a number, and \"{}\" is not one.", Inspector::joinPath(path), text));
        }

        if (std::trunc(number) == number && std::abs(number) < 9.0e15) {
            lua_pushinteger(L, static_cast<lua_Integer>(number));
        } else {
            lua_pushnumber(L, number);
        }
    } else if (current == LUA_TBOOLEAN) {
        if (text != "true" && text != "false") {
            throw std::invalid_argument(std::format("The value at \"{}\" is a boolean, which is \"true\" or \"false\".", Inspector::joinPath(path)));
        }

        lua_pushboolean(L, text == "true" ? 1 : 0);
    } else if (current == LUA_TSTRING) {
        lua_pushlstring(L, text.data(), text.size());
    } else {
        throw std::invalid_argument(std::format("The value at \"{}\" is of the type \"{}\", which the inspector does not edit. It edits numbers, booleans and strings.", Inspector::joinPath(path), lua_typename(L, current)));
    }

    lua_settable(L, container);
}

void ScriptInspector::watch(lua_State* L, std::string_view name, int index) {
    const int value = lua_absindex(L, index);
    pushWatched(L);
    lua_pushlstring(L, name.data(), name.size());
    lua_pushvalue(L, value);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

// The table of watched values holds them weakly, so watching never keeps an owner alive.
void ScriptInspector::pushWatched(lua_State* L) {
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

void ScriptInspector::pushRoot(lua_State* L, std::string_view name) {
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

void ScriptInspector::pushModuleField(lua_State* L, const char* module, const char* field, bool call) {
    lua_getglobal(L, "require");
    lua_pushstring(L, module);
    lua_call(L, 1, 1);
    lua_getfield(L, -1, field);
    lua_remove(L, -2);
    if (call) {
        lua_call(L, 0, 1);
    }
}

void ScriptInspector::pushPath(lua_State* L, std::span<const std::string> path) {
    pushRoot(L, path.front());
    for (std::size_t index = 1; index < path.size(); ++index) {
        const int container = lua_gettop(L);
        if (!lua_istable(L, container) && !lua_isuserdata(L, container)) {
            throw std::invalid_argument(std::format("The value at \"{}\" is of the type \"{}\", which holds no \"{}\".", Inspector::joinPath(path.first(index)), luaL_typename(L, container), path[index]));
        }

        pushKey(L, container, path[index]);
        lua_gettable(L, container);
        lua_remove(L, container);
    }
}

// A key that reads as an integer names the item of a sequence when the table holds no text key of that name.
void ScriptInspector::pushKey(lua_State* L, int container, const std::string& key) {
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
std::optional<std::int64_t> ScriptInspector::readIndex(std::string_view key) noexcept {
    std::int64_t number = 0;
    const char* end = key.data() + key.size();
    const std::from_chars_result read = std::from_chars(key.data(), end, number);
    if (read.ec != std::errc{} || read.ptr != end) {
        return std::nullopt;
    }

    return number;
}

void ScriptInspector::describe(lua_State* L, int index, bool editable, Inspector::Entry& entry) {
    const int type = lua_type(L, index);
    entry.type = lua_typename(L, type);
    if (type == LUA_TUSERDATA && lua_getmetatable(L, index) != 0) {
        if (lua_getfield(L, -1, "__name") == LUA_TSTRING) {
            entry.type = lua_tostring(L, -1);
        }

        lua_pop(L, 2);
    }

    describeValue(L, index, entry.value);
    const bool primitive = type == LUA_TNUMBER || type == LUA_TBOOLEAN || type == LUA_TSTRING;
    entry.editable = editable && primitive;
    entry.expandable = type == LUA_TTABLE || type == LUA_TUSERDATA;
}

void ScriptInspector::describeValue(lua_State* L, int index, std::string& text) {
    text.clear();
    switch (lua_type(L, index)) {
    case LUA_TNUMBER:
        appendNumber(L, index, text);
        break;
    case LUA_TBOOLEAN:
        text = lua_toboolean(L, index) != 0 ? "true" : "false";
        break;
    case LUA_TSTRING:
        appendText(L, index, Inspector::kMaxValueText, text);
        break;
    case LUA_TTABLE:
        describeTable(L, index, text);
        break;
    case LUA_TNIL:
        text = "nil";
        break;
    default:
        break;
    }
}

// Counts the values of a table up to the most that a listing shows, since a table that is not a sequence has no length.
void ScriptInspector::describeTable(lua_State* L, int index, std::string& text) {
    const int table = lua_absindex(L, index);
    std::size_t count = 0;
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        lua_pop(L, 1);
        if (++count > Inspector::kMaxEntries) {
            lua_pop(L, 1);
            std::format_to(std::back_inserter(text), "More than {} values", Inspector::kMaxEntries);
            return;
        }
    }

    if (count == 0) {
        text = "Empty";
        return;
    }

    // A table with a name, such as a scene or an object of the app, shows it before its count.
    lua_pushliteral(L, "name");
    if (lua_rawget(L, table) == LUA_TSTRING) {
        appendText(L, -1, Inspector::kMaxValueText, text);
        text += ", ";
    }

    lua_pop(L, 1);

    if (count == 1) {
        text += "1 value";
        return;
    }

    std::format_to(std::back_inserter(text), "{} values", count);
}

// The shortest text that reads back as the same number, which the formatting of the library gives too, written through a buffer on the stack, since the formatting of a floating point number may allocate.
void ScriptInspector::appendNumber(lua_State* L, int index, std::string& text) {
    std::array<char, 32> digits{};
    const std::to_chars_result written = lua_isinteger(L, index) != 0 ? std::to_chars(digits.data(), digits.data() + digits.size(), lua_tointeger(L, index)) : std::to_chars(digits.data(), digits.data() + digits.size(), lua_tonumber(L, index));
    text.append(digits.data(), written.ptr);
}

// Keys are read without converting them in place, which would break the traversal of the table.
void ScriptInspector::describeKey(lua_State* L, int index, std::string& text) {
    text.clear();
    switch (lua_type(L, index)) {
    case LUA_TSTRING:
        appendText(L, index, std::numeric_limits<std::size_t>::max(), text);
        break;
    case LUA_TNUMBER:
        appendNumber(L, index, text);
        break;
    default:
        std::format_to(std::back_inserter(text), "({})", luaL_typename(L, index));
        break;
    }
}

// A Lua string holds any bytes, and every description reaches JSON, so the text keeps whole characters within the limit and replaces bytes that are not UTF-8.
void ScriptInspector::appendText(lua_State* L, int index, std::size_t limit, std::string& text) {
    std::size_t length = 0;
    const char* characters = lua_tolstring(L, index, &length);
    const std::string_view bytes(characters, length);
    const std::size_t start = text.size();

    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t before = text.size();
        core::Utf8::append(text, core::Utf8::decode(bytes, offset));
        if (text.size() - start > limit) {
            text.resize(before);
            break;
        }
    }
}

} // namespace haylen::debug
