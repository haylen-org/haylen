#include "debug/HaltedStack.hpp"

#include <algorithm>
#include <format>
#include <string_view>

#include "debug/Evaluation.hpp"

namespace haylen::debug {

HaltedStack::HaltedStack(lua_State* thread, int firstLevel) : state(thread), first(firstLevel) {
    lua_newtable(state);
    values = luaL_ref(state, LUA_REGISTRYINDEX);
}

HaltedStack::~HaltedStack() {
    luaL_unref(state, LUA_REGISTRYINDEX, values);
}

bool HaltedStack::findLevel(int frameId, lua_Debug& level) const {
    return frameId >= 1 && lua_getstack(state, first + frameId - 1, &level) != 0;
}

int HaltedStack::addHandle(Handle handle) {
    handles.push_back(handle);
    return static_cast<int>(handles.size());
}

std::string HaltedStack::describeFunction(const lua_Debug& level) {
    if (level.name != nullptr && level.name[0] != '\0') {
        return level.name;
    }
    if (std::string_view(level.what) == "main") {
        return "main chunk";
    }
    if (std::string_view(level.what) == "C") {
        return "[C]";
    }
    return "function <" + std::string(level.short_src) + ":" + std::to_string(level.linedefined) + ">";
}

// A frame of a file of the package names its package path, which the development server turns into the path on the machine of the developer, while code from a text, such as an evaluation, has a name alone.
core::Json HaltedStack::getFrames(int start, int levels) const {
    core::Json frames = core::Json::array();
    lua_Debug level{};
    int total = 0;
    for (int frameId = 1; findLevel(frameId, level); ++frameId) {
        ++total;
        if (frameId <= start || (levels > 0 && static_cast<int>(frames.size()) >= levels)) {
            continue;
        }
        lua_getinfo(state, "Sln", &level);
        core::Json frame{{"id", frameId}, {"name", describeFunction(level)}, {"line", std::max(level.currentline, 0)}, {"column", level.currentline > 0 ? 1 : 0}};
        if (level.source[0] == '@') {
            const std::string_view path(level.source + 1);
            frame["source"] = {{"name", std::string(path.substr(path.rfind('/') + 1))}, {"path", std::string(path)}};
        } else if (std::string_view(level.what) != "C") {
            frame["source"] = {{"name", level.short_src}};
        } else {
            frame["presentationHint"] = "subtle";
        }
        frames.push_back(std::move(frame));
    }
    return {{"stackFrames", std::move(frames)}, {"totalFrames", total}};
}

core::Json HaltedStack::getScopes(int frameId) {
    lua_Debug level{};
    if (!findLevel(frameId, level)) {
        return nullptr;
    }
    const int depth = first + frameId - 1;
    return {{"scopes",
             {
                 {{"name", "Locals"}, {"presentationHint", "locals"}, {"variablesReference", addHandle({.kind = Kind::Locals, .level = depth})}, {"expensive", false}},
                 {{"name", "Upvalues"}, {"variablesReference", addHandle({.kind = Kind::Upvalues, .level = depth})}, {"expensive", false}},
                 {{"name", "Globals"}, {"variablesReference", addHandle({.kind = Kind::Globals, .level = depth})}, {"expensive", true}},
             }}};
}

core::Json HaltedStack::getVariables(int reference, int start, int count) {
    if (reference < 1 || static_cast<std::size_t>(reference) > handles.size()) {
        return nullptr;
    }
    luaL_checkstack(state, 8, "The stack is too deep to inspect.");
    const int top = lua_gettop(state);
    const Handle handle = handles[static_cast<std::size_t>(reference) - 1];
    std::vector<Child> children;
    if (handle.kind == Kind::Locals) {
        listLocals(handle, children);
    } else if (handle.kind == Kind::Value) {
        lua_rawgeti(state, LUA_REGISTRYINDEX, values);
        lua_rawgeti(state, -1, handle.slot);
        const int value = lua_gettop(state);
        if (lua_type(state, value) == LUA_TTABLE) {
            listTable(value, children);
        } else if (lua_type(state, value) == LUA_TUSERDATA) {
            listUserdata(value, children);
        } else if (lua_type(state, value) == LUA_TFUNCTION) {
            listUpvalues(value, children);
        }
    } else {
        lua_Debug level{};
        if (lua_getstack(state, handle.level, &level) != 0 && lua_getinfo(state, "f", &level) != 0) {
            const int function = lua_gettop(state);
            if (handle.kind == Kind::Upvalues) {
                listUpvalues(function, children);
            } else {
                pushGlobals(state, function);
                listTable(lua_gettop(state), children);
            }
        }
    }
    lua_settop(state, top);

    // Locals and upvalues keep the order of the code, and the values of a table sort by their keys.
    if (handle.kind != Kind::Locals && handle.kind != Kind::Upvalues) {
        // clang-format off
        std::ranges::stable_sort(children, [](const Child& left, const Child& right) {
            if (left.numbered != right.numbered) {
                return left.numbered;
            }
            return left.numbered ? left.number < right.number : left.name < right.name;
        });
        // clang-format on
    }
    core::Json variables = core::Json::array();
    const int from = std::max(start, 0);
    const int limit = count > 0 ? count : kMaxChildren;
    for (int index = from; index < static_cast<int>(children.size()) && static_cast<int>(variables.size()) < limit; ++index) {
        variables.push_back(std::move(children[static_cast<std::size_t>(index)].variable));
    }
    return {{"variables", std::move(variables)}};
}

// Names in parentheses are the temporaries of Lua, while the extra arguments of a function with `...` count from 1.
void HaltedStack::listLocals(const Handle& handle, std::vector<Child>& children) {
    lua_Debug level{};
    if (lua_getstack(state, handle.level, &level) == 0) {
        return;
    }
    for (int slot = 1; const char* name = lua_getlocal(state, &level, slot); ++slot) {
        if (name[0] != '(') {
            children.push_back({.name = name, .variable = describe(name, -1)});
        }
        lua_pop(state, 1);
    }
    for (int slot = -1; lua_getlocal(state, &level, slot) != nullptr; --slot) {
        const std::string name = "(vararg " + std::to_string(-slot) + ")";
        children.push_back({.name = name, .variable = describe(name, -1)});
        lua_pop(state, 1);
    }
}

// The upvalue `_ENV` holds the globals of the function, which the scope of the globals shows.
void HaltedStack::listUpvalues(int function, std::vector<Child>& children) {
    for (int slot = 1; const char* name = lua_getupvalue(state, function, slot); ++slot) {
        const std::string text = name[0] == '\0' ? "(upvalue " + std::to_string(slot) + ")" : name;
        if (text != "_ENV") {
            children.push_back({.name = text, .variable = describe(text, -1)});
        }
        lua_pop(state, 1);
    }
}

void HaltedStack::listTable(int table, std::vector<Child>& children) {
    if (lua_getmetatable(state, table) != 0) {
        children.push_back({.name = "(metatable)", .variable = describe("(metatable)", -1)});
        lua_pop(state, 1);
    }
    lua_pushnil(state);
    while (lua_next(state, table) != 0) {
        Child child{.numbered = lua_isinteger(state, -2) != 0, .name = describeKey(state, -2)};
        if (child.numbered) {
            child.number = lua_tointeger(state, -2);
        }
        child.variable = describe(child.name, -1);
        children.push_back(std::move(child));
        lua_pop(state, 1);
    }
}

// An object of a bound type lists the properties that its type declares, which the native getters of its binding read, so no code of the app runs.
void HaltedStack::listUserdata(int userdata, std::vector<Child>& children) {
    if (lua_getmetatable(state, userdata) != 0) {
        children.push_back({.name = "(metatable)", .variable = describe("(metatable)", -1)});
        lua_pushliteral(state, "__properties");
        if (lua_rawget(state, -2) == LUA_TTABLE) {
            listProperties(userdata, lua_gettop(state), children);
        }
        lua_pop(state, 2);
    }
    for (int slot = 1; lua_getiuservalue(state, userdata, slot) != LUA_TNONE; ++slot) {
        const std::string name = "(user value " + std::to_string(slot) + ")";
        children.push_back({.name = name, .variable = describe(name, -1)});
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
}

void HaltedStack::listProperties(int userdata, int getters, std::vector<Child>& children) {
    lua_pushnil(state);
    while (lua_next(state, getters) != 0) {
        if (lua_type(state, -2) != LUA_TSTRING || lua_iscfunction(state, -1) == 0) {
            lua_pop(state, 1);
            continue;
        }
        const std::string name = lua_tostring(state, -2);
        lua_pushvalue(state, userdata);
        lua_pushvalue(state, -3);
        if (lua_pcall(state, 2, 1, 0) == LUA_OK) {
            children.push_back({.name = name, .variable = describe(name, -1)});
        } else {
            children.push_back({.name = name, .variable = {{"name", name}, {"value", "(" + Evaluation::describeError(state, -1) + ")"}, {"variablesReference", 0}}});
        }
        lua_pop(state, 1);
    }
}

void HaltedStack::pushGlobals(lua_State* L, int function) {
    for (int slot = 1; const char* name = lua_getupvalue(L, function, slot); ++slot) {
        if (std::string_view(name) == "_ENV") {
            return;
        }
        lua_pop(L, 1);
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
}

core::Json HaltedStack::describe(const std::string& name, int index) {
    const int value = lua_absindex(state, index);
    core::Json variable{{"name", name}, {"value", formatValue(state, value)}, {"type", luaL_typename(state, value)}, {"variablesReference", 0}};
    const int type = lua_type(state, value);
    if (type == LUA_TTABLE || type == LUA_TUSERDATA || (type == LUA_TFUNCTION && lua_getupvalue(state, value, 1) != nullptr)) {
        if (type == LUA_TFUNCTION) {
            lua_pop(state, 1);
        }
        lua_rawgeti(state, LUA_REGISTRYINDEX, values);
        lua_pushvalue(state, value);
        lua_rawseti(state, -2, ++valueCount);
        lua_pop(state, 1);
        variable["variablesReference"] = addHandle({.kind = Kind::Value, .slot = valueCount});
    }
    return variable;
}

// A copy of a number turns into its text the way Lua prints it, and the name of a type comes from the field `__name` of its metatable, read raw.
std::string HaltedStack::formatValue(lua_State* L, int index) {
    const int value = lua_absindex(L, index);
    switch (lua_type(L, value)) {
    case LUA_TNIL:
        return "nil";
    case LUA_TBOOLEAN:
        return lua_toboolean(L, value) != 0 ? "true" : "false";
    case LUA_TNUMBER: {
        lua_pushvalue(L, value);
        std::string text = lua_tostring(L, -1);
        lua_pop(L, 1);
        return text;
    }
    case LUA_TSTRING: {
        std::size_t length = 0;
        const char* text = lua_tolstring(L, value, &length);
        if (length > kMaxText) {
            return quote({text, kMaxText}) + "... (" + std::to_string(length) + " bytes)";
        }
        return quote({text, length});
    }
    default:
        break;
    }

    std::string kind = luaL_typename(L, value);
    if (lua_getmetatable(L, value) != 0) {
        lua_pushliteral(L, "__name");
        if (lua_rawget(L, -2) == LUA_TSTRING) {
            kind = lua_tostring(L, -1);
        }
        lua_pop(L, 2);
    }
    if (lua_type(L, value) == LUA_TFUNCTION) {
        lua_Debug info{};
        lua_pushvalue(L, value);
        lua_getinfo(L, ">S", &info);
        if (std::string_view(info.what) != "C") {
            return "function: " + std::string(info.short_src) + ":" + std::to_string(info.linedefined);
        }
    }
    return std::format("{}: {}", kind, lua_topointer(L, value));
}

std::string HaltedStack::describeKey(lua_State* L, int index) {
    const int key = lua_absindex(L, index);
    if (lua_type(L, key) == LUA_TSTRING) {
        return lua_tostring(L, key);
    }
    return "[" + formatValue(L, key) + "]";
}

std::string HaltedStack::quote(std::string_view text) {
    std::string quoted = "\"";
    for (const char character : text) {
        if (character == '"' || character == '\\') {
            quoted += '\\';
            quoted += character;
        } else if (character == '\n') {
            quoted += "\\n";
        } else if (character == '\r') {
            quoted += "\\r";
        } else if (character == '\t') {
            quoted += "\\t";
        } else {
            quoted += character;
        }
    }
    return quoted + "\"";
}

// The names of the frame go into the environment from the outside in, so a local shadows an upvalue of the same name and a later local an earlier one. A name whose value is nil still belongs to the frame, so it never reads the global of the same name.
bool HaltedStack::pushEnvironment(int frameId) {
    lua_Debug level{};
    if (!findLevel(frameId, level)) {
        return false;
    }
    luaL_checkstack(state, 8, "The stack is too deep to evaluate in.");
    lua_getinfo(state, "f", &level);
    const int function = lua_gettop(state);
    lua_newtable(state);
    const int environment = lua_gettop(state);
    lua_newtable(state);
    const int names = lua_gettop(state);
    // clang-format off
    const auto add = [this, environment, names](const char* name) {
        lua_pushstring(state, name);
        lua_pushboolean(state, 1);
        lua_rawset(state, names);
        lua_pushstring(state, name);
        lua_insert(state, -2);
        lua_rawset(state, environment);
    };
    // clang-format on
    for (int slot = 1; const char* name = lua_getupvalue(state, function, slot); ++slot) {
        if (name[0] != '\0' && std::string_view(name) != "_ENV") {
            add(name);
        } else {
            lua_pop(state, 1);
        }
    }
    for (int slot = 1; const char* name = lua_getlocal(state, &level, slot); ++slot) {
        if (name[0] != '(') {
            add(name);
        } else {
            lua_pop(state, 1);
        }
    }

    lua_createtable(state, 0, 2);
    lua_pushvalue(state, names);
    pushGlobals(state, function);
    lua_pushcclosure(state, &readName, 2);
    lua_setfield(state, -2, "__index");
    lua_pushvalue(state, names);
    pushGlobals(state, function);
    lua_pushcclosure(state, &writeName, 2);
    lua_setfield(state, -2, "__newindex");
    lua_setmetatable(state, environment);
    lua_settop(state, environment);
    lua_remove(state, function);
    return true;
}

int HaltedStack::readName(lua_State* L) {
    lua_pushvalue(L, 2);
    if (lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushvalue(L, 2);
    lua_gettable(L, lua_upvalueindex(2));
    return 1;
}

int HaltedStack::writeName(lua_State* L) {
    lua_pushvalue(L, 2);
    if (lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) {
        lua_settop(L, 3);
        lua_rawset(L, 1);
        return 0;
    }
    lua_settop(L, 3);
    lua_settable(L, lua_upvalueindex(2));
    return 0;
}

} // namespace haylen::debug
