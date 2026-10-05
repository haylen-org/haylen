#include "lua/ModuleReloader.hpp"

#include <array>
#include <utility>

#include "haylen/core/Log.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Runtime.hpp"
#include "lua/ModuleClosures.hpp"
#include "lua/ModuleGraph.hpp"
#include "lua/ModuleSnapshot.hpp"
#include "lua/Owners.hpp"
#include "lua/ValueRemap.hpp"

namespace haylen::lua {

ModuleReloader::ModuleReloader(lua_State* state, ModuleGraph& modules, const io::Package& source) : L(state), graph(modules), package(source) {
    lua_newtable(L);
    replacements = Reference(L, -1);
    lua_newtable(L);
    patched = Reference(L, -1);
    lua_pop(L, 2);
}

std::string_view ModuleReloader::getKind(lua_State* L, int index) noexcept {
    return lua_typename(L, lua_type(L, index));
}

ModuleReloader::Result ModuleReloader::reload(const std::string& file) {
    ModuleGraph::Module& module = *graph.find(file);
    path = file;
    name = module.names.front();
    if (!package.exists(file)) {
        return {.status = Status::Restart, .reason = "The module file \"" + file + "\" was removed."};
    }
    const std::string source = package.readText(file);
    if (source == module.source) {
        return {};
    }
    if (module.restartOnChange) {
        return {.status = Status::Restart, .reason = "The module \"" + name + "\" restarts the app when it changes."};
    }

    const int top = lua_gettop(L);
    const std::string chunkName = "@" + file;
    if (luaL_loadbufferx(L, source.data(), source.size(), chunkName.c_str(), "t") != LUA_OK) {
        Error error(lua_tostring(L, -1));
        lua_settop(L, top);
        return {.status = Status::Failed, .error = std::move(error)};
    }
    const int chunk = top + 1;

    // The globals that the new load writes wait in a staging table, and what it registers belongs to a staging owner, until it ran without errors.
    lua_newtable(L);
    const int staging = top + 2;
    lua_createtable(L, 0, 1);
    lua_pushglobaltable(L);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, staging);
    lua_pushvalue(L, staging);
    ModuleGraph::setEnvironment(L, chunk);
    lua_newtable(L);
    const int owner = top + 3;
    // clang-format off
    const auto abandon = [this, top, chunk, owner] {
        Owners::release(L, owner);
        lua_pushglobaltable(L);
        ModuleGraph::setEnvironment(L, chunk);
        lua_settop(L, top);
    };
    // clang-format on

    graph.beginLoad(L, file, owner);
    lua_pushvalue(L, chunk);
    lua_pushstring(L, name.c_str());
    lua_pushstring(L, file.c_str());
    try {
        Runtime::protectedCall(L, 2, 1);
    } catch (const Error& error) {
        graph.endLoad(L);
        abandon();
        return {.status = Status::Failed, .error = error};
    }
    graph.endLoad(L);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }
    const int fresh = top + 4;
    lua_pushnil(L);
    lua_setmetatable(L, staging);

    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_getfield(L, -1, name.c_str());
    lua_remove(L, -2);
    const int live = top + 5;
    ModuleGraph::pushSnapshot(L, file);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    lua_getfield(L, top + 6, "value");
    lua_getfield(L, top + 6, "upvalues");
    lua_getfield(L, top + 6, "globals");
    const int baseValue = top + 7;
    const int baseUpvalues = top + 8;
    const int baseGlobals = top + 9;

    const bool tables = lua_istable(L, live) && lua_istable(L, fresh);
    const bool functions = lua_isfunction(L, live) && lua_isfunction(L, fresh);
    const bool empty = lua_isboolean(L, live) && lua_toboolean(L, live) != 0 && lua_isboolean(L, fresh) && lua_toboolean(L, fresh) != 0;
    if (!tables && !functions && !empty) {
        std::string reason = "The module \"" + name + "\" now returns a " + std::string(getKind(L, fresh)) + " instead of a " + std::string(getKind(L, live)) + ".";
        abandon();
        return {.status = Status::Restart, .reason = std::move(reason)};
    }

    ModuleGraph::pushOwner(L, file);
    const int previousOwner = top + 10;
    Owners::pushFunctions(L, previousOwner);
    Owners::pushFunctions(L, owner);
    pushPreviousGlobals(baseGlobals);
    const std::array<int, 3> previousRoots{live, top + 13, top + 11};
    const ModuleClosures previous(L, file, previousRoots);
    const std::array<int, 3> freshRoots{fresh, staging, top + 12};
    const ModuleClosures created(L, file, freshRoots);
    for (const ModuleClosures* closures : {&previous, &created}) {
        if (const std::string variable = closures->findAmbiguous(); !variable.empty()) {
            abandon();
            return {.status = Status::Restart, .reason = "The module \"" + name + "\" has two different variables named \"" + variable + "\" that its functions capture."};
        }
    }

    // The values the new load produced become the base of the next reload, so they are read before the merge changes what its functions see.
    created.pushValues(L);
    const int freshUpvalues = top + 14;
    if (tables && lua_rawequal(L, live, fresh) != 0) {
        mapReplaced(live, baseValue, 0);
    } else if (tables) {
        mergeTable(live, fresh, baseValue, name);
    } else if (functions) {
        mapFunction(live, fresh);
    }
    joinVariables(previous, created, baseUpvalues);
    mergeGlobals(staging, baseGlobals);
    lua_pushglobaltable(L);
    ModuleGraph::setEnvironment(L, chunk);

    Owners::release(L, previousOwner);
    ModuleGraph::setOwner(L, file, owner);
    ModuleGraph::pushCreators(L);
    ModuleSnapshot::push(L, fresh, freshUpvalues, staging, lua_gettop(L), file);
    ModuleGraph::setSnapshot(L, file, -1);
    module.source = source;
    lua_settop(L, top);
    return {.status = Status::Patched};
}

void ModuleReloader::finish() {
    replacements.push(L);
    const int map = lua_gettop(L);
    ModuleGraph::pushStore(L);
    patched.push(L);
    lua_pushnil(L);
    const bool changed = lua_next(L, map) != 0;
    if (!changed) {
        lua_settop(L, map - 1);
        lua_newtable(L);
        return;
    }
    lua_pop(L, 2);
    ValueRemap::run(L, map, map + 1, map + 2);
    lua_replace(L, map);
    lua_settop(L, map);
}

// Code always comes from the new load, unless the app put another function in place of the one of the last load. Engine objects and instances keep their identity, tables of the module merge field by field, and any other value takes the edited literal only while the app left it as the last load made it.
void ModuleReloader::pushMerged(int base, int live, int fresh, const std::string& field) {
    const int previous = lua_absindex(L, base);
    const int current = lua_absindex(L, live);
    const int next = lua_absindex(L, fresh);
    const int freshType = lua_type(L, next);
    const int liveType = lua_type(L, current);
    if (freshType == LUA_TFUNCTION) {
        mapFunction(previous, next);
        const bool replacedByApp = liveType == LUA_TFUNCTION && lua_isfunction(L, previous) && lua_rawequal(L, current, previous) == 0;
        if (!replacedByApp) {
            mapFunction(current, next);
        }
        lua_pushvalue(L, replacedByApp ? current : next);
        return;
    }
    if (liveType == LUA_TUSERDATA || liveType == LUA_TTHREAD) {
        lua_pushvalue(L, current);
        return;
    }
    if (liveType == LUA_TTABLE && freshType == LUA_TTABLE) {
        if (lua_rawequal(L, current, next) != 0) {
            mapReplaced(current, previous, 0);
            lua_pushvalue(L, current);
            return;
        }
        if (!ModuleSnapshot::isMergeable(L, current)) {
            lua_pushvalue(L, current);
            return;
        }
        if (ModuleSnapshot::isMergeable(L, next) && isClaimed(current, true) && !isClaimed(next, false)) {
            mergeTable(current, next, previous, field);
            lua_pushvalue(L, current);
            return;
        }
    }

    const bool untouched = ModuleSnapshot::equals(L, current, previous);
    if (!untouched && liveType != freshType && freshType != LUA_TNIL) {
        core::Log::warning("The field \"{}\" of the module \"{}\" keeps its value, because the app changed it and the new code gives it another kind.", field, name);
    }
    lua_pushvalue(L, untouched ? next : current);
}

// A pair already merged is never merged again, which ends cycles such as a class whose `__index` is the class itself.
void ModuleReloader::mergeTable(int live, int fresh, int base, const std::string& field) {
    const int current = lua_absindex(L, live);
    const int next = lua_absindex(L, fresh);
    const int previous = lua_absindex(L, base);
    replacements.push(L);
    const int map = lua_gettop(L);
    lua_pushvalue(L, next);
    if (lua_rawget(L, map) != LUA_TNIL) {
        lua_pop(L, 2);
        return;
    }
    lua_pop(L, 1);
    lua_pushvalue(L, next);
    lua_pushvalue(L, current);
    lua_rawset(L, map);
    patched.push(L);
    lua_pushvalue(L, current);
    lua_pushboolean(L, 1);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    const bool hasBase = lua_istable(L, previous);

    lua_pushnil(L);
    while (lua_next(L, next) != 0) {
        const int value = lua_gettop(L);
        const int key = value - 1;

        // A key that is a staging table stands for the live table it merged into.
        lua_pushvalue(L, key);
        if (lua_istable(L, key)) {
            lua_pushvalue(L, key);
            if (lua_rawget(L, map) != LUA_TNIL) {
                lua_replace(L, -2);
            } else {
                lua_pop(L, 1);
            }
        }
        const int liveKey = lua_gettop(L);
        if (hasBase) {
            lua_pushvalue(L, liveKey);
            lua_rawget(L, previous);
        } else {
            lua_pushnil(L);
        }
        lua_pushvalue(L, liveKey);
        lua_rawget(L, current);
        const std::string member = lua_type(L, key) == LUA_TSTRING ? field + "." + lua_tostring(L, key) : field + "[]";
        pushMerged(liveKey + 1, liveKey + 2, value, member);
        lua_pushvalue(L, liveKey);
        lua_insert(L, -2);
        lua_rawset(L, current);
        lua_settop(L, key);
    }

    // A field that the new source no longer has goes away, unless the app changed it since the last load.
    if (hasBase) {
        lua_newtable(L);
        const int removed = lua_gettop(L);
        int count = 0;
        lua_pushnil(L);
        while (lua_next(L, current) != 0) {
            const int key = lua_gettop(L) - 1;
            const int keyType = lua_type(L, key);
            if (keyType == LUA_TSTRING || keyType == LUA_TNUMBER || keyType == LUA_TBOOLEAN) {
                lua_pushvalue(L, key);
                const bool absent = lua_rawget(L, next) == LUA_TNIL;
                lua_pushvalue(L, key);
                lua_rawget(L, previous);
                if (absent && !lua_isnil(L, -1) && ModuleSnapshot::equals(L, key + 1, -1)) {
                    lua_pushvalue(L, key);
                    lua_rawseti(L, removed, ++count);
                }
            }
            lua_settop(L, key);
        }
        for (int index = 1; index <= count; ++index) {
            lua_rawgeti(L, removed, index);
            lua_pushnil(L);
            lua_rawset(L, current);
        }
        lua_pop(L, 1);
    }
    mergeMetatables(current, next, field);
    lua_settop(L, map - 1);
}

void ModuleReloader::mergeMetatables(int live, int fresh, const std::string& field) {
    const int top = lua_gettop(L);
    const bool liveHas = lua_getmetatable(L, live) != 0;
    const int liveMeta = lua_gettop(L);
    const bool freshHas = lua_getmetatable(L, fresh) != 0;
    const int freshMeta = lua_gettop(L);
    if (liveHas && freshHas && lua_rawequal(L, liveMeta, freshMeta) == 0 && ModuleSnapshot::isMergeable(L, liveMeta) && ModuleSnapshot::isMergeable(L, freshMeta)) {
        lua_pushnil(L);
        mergeTable(liveMeta, freshMeta, lua_gettop(L), field + " metatable");

        // The tables a merged metatable belongs to, such as the classes themselves, are no instances whose hooks run.
        patched.push(L);
        lua_pushvalue(L, liveMeta);
        lua_pushnil(L);
        lua_rawset(L, -3);
    } else if (!liveHas && freshHas) {
        lua_pushvalue(L, freshMeta);
        lua_setmetatable(L, live);
    }
    lua_settop(L, top);
}

void ModuleReloader::mapReplaced(int live, int base, int depth) {
    if (!lua_istable(L, base) || depth > 8) {
        return;
    }
    const int current = lua_absindex(L, live);
    const int previous = lua_absindex(L, base);
    lua_pushnil(L);
    while (lua_next(L, previous) != 0) {
        const int value = lua_gettop(L);
        lua_pushvalue(L, value - 1);
        lua_rawget(L, current);
        if (lua_isfunction(L, value) && lua_isfunction(L, -1)) {
            mapFunction(value, -1);
        } else if (lua_istable(L, value) && ModuleSnapshot::isMergeable(L, -1)) {
            mapReplaced(-1, value, depth + 1);
        }
        lua_settop(L, value - 1);
    }
}

// Only functions of the module file map, so a field that now holds a function of another module never replaces that function everywhere.
void ModuleReloader::mapFunction(int previous, int fresh) {
    const int old = lua_absindex(L, previous);
    const int next = lua_absindex(L, fresh);
    if (!lua_isfunction(L, old) || lua_rawequal(L, old, next) != 0 || !isOfModule(old)) {
        return;
    }
    replacements.push(L);
    lua_pushvalue(L, old);
    lua_pushvalue(L, next);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

bool ModuleReloader::isOfModule(int function) {
    if (lua_iscfunction(L, function) != 0) {
        return false;
    }
    lua_Debug info{};
    lua_pushvalue(L, function);
    lua_getinfo(L, ">S", &info);
    return info.source[0] == '@' && path == info.source + 1;
}

bool ModuleReloader::isClaimed(int index, bool byModule) {
    const int table = lua_absindex(L, index);
    ModuleGraph::pushCreators(L);
    lua_pushvalue(L, table);
    lua_rawget(L, -2);
    const bool claimed = byModule ? lua_type(L, -1) == LUA_TSTRING && path == lua_tostring(L, -1) : !lua_isnil(L, -1);
    lua_pop(L, 2);
    return claimed;
}

// The new functions take over the variables of the old functions with the same names, after the old variables took the merged values, so old closures, such as a coroutine suspended in old code, and new closures share the state of the module from now on.
void ModuleReloader::joinVariables(const ModuleClosures& previous, const ModuleClosures& fresh, int base) {
    const int baseIndex = lua_absindex(L, base);
    for (const auto& [variable, cells] : fresh.getCells()) {
        const auto old = previous.getCells().find(variable);
        if (old == previous.getCells().end()) {
            continue;
        }
        const ModuleClosures::Cell& target = old->second.front();
        const int mark = lua_gettop(L);
        previous.pushClosure(L, target);
        const int oldClosure = lua_gettop(L);
        lua_getupvalue(L, oldClosure, target.upvalue);
        fresh.pushClosure(L, cells.front());
        lua_getupvalue(L, -1, cells.front().upvalue);
        lua_remove(L, -2);
        if (lua_istable(L, baseIndex)) {
            lua_getfield(L, baseIndex, variable.c_str());
        } else {
            lua_pushnil(L);
        }
        pushMerged(mark + 4, mark + 2, mark + 3, "local " + variable);
        lua_setupvalue(L, oldClosure, target.upvalue);
        for (const ModuleClosures::Cell& cell : cells) {
            fresh.pushClosure(L, cell);
            lua_upvaluejoin(L, -1, cell.upvalue, oldClosure, target.upvalue);
            lua_pop(L, 1);
        }
        lua_settop(L, mark);
    }
}

void ModuleReloader::mergeGlobals(int staging, int base) {
    const int written = lua_absindex(L, staging);
    const int previous = lua_absindex(L, base);
    const bool hasBase = lua_istable(L, previous);
    lua_pushglobaltable(L);
    const int globals = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, written) != 0) {
        const int value = lua_gettop(L);
        const int key = value - 1;
        if (hasBase) {
            lua_pushvalue(L, key);
            lua_rawget(L, previous);
        } else {
            lua_pushnil(L);
        }
        lua_pushvalue(L, key);
        lua_rawget(L, globals);
        const std::string global = lua_type(L, key) == LUA_TSTRING ? std::string("global ") + lua_tostring(L, key) : std::string("global");
        pushMerged(value + 1, value + 2, value, global);
        lua_pushvalue(L, key);
        lua_insert(L, -2);
        lua_rawset(L, globals);
        lua_settop(L, key);
    }

    // A global that the new source no longer writes goes away, unless the app changed it since the last load.
    if (hasBase) {
        lua_pushnil(L);
        while (lua_next(L, previous) != 0) {
            const int value = lua_gettop(L);
            const int key = value - 1;
            lua_pushvalue(L, key);
            const bool absent = lua_rawget(L, written) == LUA_TNIL;
            lua_pushvalue(L, key);
            lua_rawget(L, globals);
            if (absent && ModuleSnapshot::equals(L, -1, value)) {
                lua_pushvalue(L, key);
                lua_pushnil(L);
                lua_rawset(L, globals);
            }
            lua_settop(L, key);
        }
    }
    lua_pop(L, 1);
}

void ModuleReloader::pushPreviousGlobals(int base) {
    const int previous = lua_absindex(L, base);
    lua_newtable(L);
    if (!lua_istable(L, previous)) {
        return;
    }
    const int result = lua_gettop(L);
    lua_pushglobaltable(L);
    lua_pushnil(L);
    while (lua_next(L, previous) != 0) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_pushvalue(L, -1);
        lua_rawget(L, result + 1);
        lua_rawset(L, result);
    }
    lua_pop(L, 1);
}

} // namespace haylen::lua
