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

ModuleReloader::ModuleReloader(lua_State* L, ModuleGraph& modules, const io::Package& source) : state(L), graph(modules), package(source) {
    lua_newtable(L);
    replacements = Reference(L, -1);
    lua_newtable(L);
    patched = Reference(L, -1);
    lua_newtable(L);
    deferred = Reference(L, -1);
    lua_pop(L, 3);
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

    const int top = lua_gettop(state);
    const std::string chunkName = "@" + file;
    if (luaL_loadbufferx(state, source.data(), source.size(), chunkName.c_str(), "t") != LUA_OK) {
        Error error(lua_tostring(state, -1));
        lua_settop(state, top);
        return {.status = Status::Failed, .error = std::move(error)};
    }
    const int chunk = top + 1;

    // The globals that the new load writes wait in a staging table, and what it registers belongs to a staging owner, until it ran without errors.
    lua_newtable(state);
    const int staging = top + 2;
    lua_createtable(state, 0, 1);
    lua_pushglobaltable(state);
    lua_setfield(state, -2, "__index");
    lua_setmetatable(state, staging);
    lua_pushvalue(state, staging);
    ModuleGraph::setEnvironment(state, chunk);
    lua_newtable(state);
    const int owner = top + 3;
    // clang-format off
    const auto abandon = [this, top, chunk, owner] {
        Owners::release(state, owner);
        lua_pushglobaltable(state);
        ModuleGraph::setEnvironment(state, chunk);
        lua_settop(state, top);
    };
    // clang-format on

    graph.beginLoad(state, file, owner);
    lua_pushvalue(state, chunk);
    lua_pushstring(state, name.c_str());
    lua_pushstring(state, file.c_str());
    try {
        Runtime::protectedCall(state, 2, 1);
    } catch (const Error& error) {
        graph.endLoad(state, false);
        abandon();
        return {.status = Status::Failed, .error = error};
    }
    graph.endLoad(state, true);
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        lua_pushboolean(state, 1);
    }
    const int fresh = top + 4;
    lua_pushnil(state);
    lua_setmetatable(state, staging);

    lua_getfield(state, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_getfield(state, -1, name.c_str());
    lua_remove(state, -2);
    const int live = top + 5;
    ModuleGraph::pushSnapshot(state, file);
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
    }
    lua_getfield(state, top + 6, "value");
    lua_getfield(state, top + 6, "upvalues");
    lua_getfield(state, top + 6, "globals");
    const int baseValue = top + 7;
    const int baseUpvalues = top + 8;
    const int baseGlobals = top + 9;

    const bool tables = lua_istable(state, live) && lua_istable(state, fresh);
    const bool functions = lua_isfunction(state, live) && lua_isfunction(state, fresh);
    const bool empty = lua_isboolean(state, live) && lua_toboolean(state, live) != 0 && lua_isboolean(state, fresh) && lua_toboolean(state, fresh) != 0;
    if (!tables && !functions && !empty) {
        const std::string freshKind(getKind(state, fresh));
        const std::string liveKind(getKind(state, live));
        std::string reason = freshKind == liveKind ? "The module \"" + name + "\" returns a " + freshKind + ", which cannot change in place." : "The module \"" + name + "\" now returns a " + freshKind + " instead of a " + liveKind + ".";
        abandon();
        return {.status = Status::Restart, .reason = std::move(reason)};
    }

    ModuleGraph::pushOwner(state, file);
    const int previousOwner = top + 10;
    Owners::pushFunctions(state, previousOwner);
    Owners::pushFunctions(state, owner);
    pushPreviousGlobals(baseGlobals);
    const std::array<int, 3> previousRoots{live, top + 13, top + 11};
    const ModuleClosures previous(state, file, previousRoots);
    const std::array<int, 3> freshRoots{fresh, staging, top + 12};
    const ModuleClosures created(state, file, freshRoots);
    for (const ModuleClosures* closures : {&previous, &created}) {
        if (const std::string variable = closures->findAmbiguous(); !variable.empty()) {
            abandon();
            return {.status = Status::Restart, .reason = "The module \"" + name + "\" has two different variables named \"" + variable + "\" that its functions capture."};
        }
    }

    // The values the new load produced become the base of the next reload, so they are read before the merge changes what its functions see.
    created.pushValues(state);
    const int freshUpvalues = top + 14;
    if (tables && lua_rawequal(state, live, fresh) != 0) {
        mapReplaced(live, baseValue, 0);
    } else if (tables) {
        mergeTable(live, fresh, baseValue, name);
    } else if (functions) {
        mapFunction(live, fresh);
    }
    joinVariables(previous, created, baseUpvalues);
    mergeGlobals(staging, baseGlobals);
    mergeDeferred();
    lua_pushglobaltable(state);
    ModuleGraph::setEnvironment(state, chunk);

    Owners::release(state, previousOwner);
    ModuleGraph::setOwner(state, file, owner);
    ModuleGraph::pushCreators(state);
    replacements.push(state);
    ModuleSnapshot::push(state, fresh, freshUpvalues, staging, lua_gettop(state) - 1, file, lua_gettop(state));
    ModuleGraph::setSnapshot(state, file, -1);
    module.source = source;
    lua_settop(state, top);
    return {.status = Status::Patched};
}

void ModuleReloader::finish() {
    replacements.push(state);
    const int map = lua_gettop(state);
    ModuleGraph::pushStore(state);
    patched.push(state);
    lua_pushnil(state);
    const bool changed = lua_next(state, map) != 0;
    if (!changed) {
        lua_settop(state, map - 1);
        lua_newtable(state);
        return;
    }
    lua_pop(state, 2);
    ValueRemap::run(state, map, map + 1, map + 2);
    lua_replace(state, map);
    lua_settop(state, map);
}

// Code always comes from the new load, unless the app put another function in place of the one of the last load. Engine objects and instances keep their identity, tables of the module merge field by field, and any other value takes the edited literal only while the app left it as the last load made it.
void ModuleReloader::pushMerged(int base, int live, int fresh, const std::string& field) {
    luaL_checkstack(state, 8, kDepthMessage);
    const int previous = lua_absindex(state, base);
    const int current = lua_absindex(state, live);
    const int next = lua_absindex(state, fresh);
    const int freshType = lua_type(state, next);
    const int liveType = lua_type(state, current);
    if (freshType == LUA_TFUNCTION) {
        mapFunction(previous, next);
        const bool replacedByApp = liveType == LUA_TFUNCTION && lua_isfunction(state, previous) && lua_rawequal(state, current, previous) == 0;
        if (!replacedByApp) {
            mapFunction(current, next);
        }
        lua_pushvalue(state, replacedByApp ? current : next);
        return;
    }
    if (liveType == LUA_TUSERDATA || liveType == LUA_TTHREAD) {
        lua_pushvalue(state, current);
        return;
    }
    if (liveType == LUA_TTABLE && freshType == LUA_TTABLE) {
        if (lua_rawequal(state, current, next) != 0) {
            mapReplaced(current, previous, 0);
            lua_pushvalue(state, current);
            return;
        }
        if (!ModuleSnapshot::isMergeable(state, current)) {
            lua_pushvalue(state, current);
            return;
        }
        if (ModuleSnapshot::isMergeable(state, next) && isClaimed(current, true) && !isClaimed(next, false)) {
            mergeTable(current, next, previous, field);
            lua_pushvalue(state, current);
            return;
        }
    }

    const bool untouched = ModuleSnapshot::equals(state, current, previous);
    if (!untouched && liveType != freshType && freshType != LUA_TNIL) {
        core::Log::warning("The field \"{}\" of the module \"{}\" keeps its value, because the app changed it and the new code gives it another kind.", field, name);
    }
    lua_pushvalue(state, untouched ? next : current);
}

// A pair already merged is never merged again, which ends cycles such as a class whose `__index` is the class itself.
void ModuleReloader::mergeTable(int live, int fresh, int base, const std::string& field) {
    luaL_checkstack(state, 8, kDepthMessage);
    const int current = lua_absindex(state, live);
    const int next = lua_absindex(state, fresh);
    const int previous = lua_absindex(state, base);
    replacements.push(state);
    const int map = lua_gettop(state);
    lua_pushvalue(state, next);
    if (lua_rawget(state, map) != LUA_TNIL) {
        lua_pop(state, 2);
        return;
    }
    lua_pop(state, 1);
    lua_pushvalue(state, next);
    lua_pushvalue(state, current);
    lua_rawset(state, map);
    patched.push(state);
    lua_pushvalue(state, current);
    lua_pushboolean(state, 1);
    lua_rawset(state, -3);
    lua_pop(state, 1);
    const bool hasBase = lua_istable(state, previous);

    lua_pushnil(state);
    while (lua_next(state, next) != 0) {
        const int value = lua_gettop(state);
        const int key = value - 1;
        if (lua_istable(state, key)) {
            defer(current, next, previous, key, field + "[]");
        } else {
            const std::string member = lua_type(state, key) == LUA_TSTRING ? field + "." + lua_tostring(state, key) : field + "[]";
            mergeField(current, previous, key, value, member);
        }
        lua_settop(state, key);
    }

    // A field that the new source no longer has goes away, unless the app changed it since the last load.
    if (hasBase) {
        lua_newtable(state);
        const int removed = lua_gettop(state);
        int count = 0;
        lua_pushnil(state);
        while (lua_next(state, current) != 0) {
            const int key = lua_gettop(state) - 1;
            const int keyType = lua_type(state, key);
            if (keyType == LUA_TSTRING || keyType == LUA_TNUMBER || keyType == LUA_TBOOLEAN) {
                lua_pushvalue(state, key);
                const bool absent = lua_rawget(state, next) == LUA_TNIL;
                lua_pushvalue(state, key);
                lua_rawget(state, previous);
                if (absent && !lua_isnil(state, -1) && ModuleSnapshot::equals(state, key + 1, -1)) {
                    lua_pushvalue(state, key);
                    lua_rawseti(state, removed, ++count);
                }
            }
            lua_settop(state, key);
        }
        for (int index = 1; index <= count; ++index) {
            lua_rawgeti(state, removed, index);
            lua_pushnil(state);
            lua_rawset(state, current);
        }
        lua_pop(state, 1);
    }
    mergeMetatables(current, next, field);
    lua_settop(state, map - 1);
}

void ModuleReloader::mergeField(int live, int base, int key, int value, const std::string& field) {
    const int top = lua_gettop(state);
    if (lua_istable(state, base)) {
        lua_pushvalue(state, key);
        lua_rawget(state, base);
    } else {
        lua_pushnil(state);
    }
    lua_pushvalue(state, key);
    lua_rawget(state, live);
    pushMerged(top + 1, top + 2, value, field);
    lua_pushvalue(state, key);
    lua_insert(state, -2);
    lua_rawset(state, live);
    lua_settop(state, top);
}

void ModuleReloader::defer(int live, int fresh, int base, int key, const std::string& field) {
    deferred.push(state);
    lua_createtable(state, 0, 5);
    for (const auto& [slot, index] : {std::pair{"live", live}, std::pair{"fresh", fresh}, std::pair{"base", base}, std::pair{"key", key}}) {
        lua_pushvalue(state, index);
        lua_setfield(state, -2, slot);
    }
    lua_pushstring(state, field.c_str());
    lua_setfield(state, -2, "field");
    lua_rawseti(state, -2, static_cast<lua_Integer>(lua_rawlen(state, -2)) + 1);
    lua_pop(state, 1);
}

// Each round merges the fields whose keys merged into live tables by then, and a round that finds none merges the rest under the keys of the new load, since nothing maps them anymore. Merging a field may defer more fields, which the next rounds take.
void ModuleReloader::mergeDeferred() {
    luaL_checkstack(state, 12, kDepthMessage);
    replacements.push(state);
    const int map = lua_gettop(state);
    bool forced = false;
    while (true) {
        deferred.push(state);
        const int round = lua_gettop(state);
        lua_newtable(state);
        deferred = Reference(state, -1);
        lua_pop(state, 1);
        const auto count = static_cast<lua_Integer>(lua_rawlen(state, round));
        if (count == 0) {
            break;
        }

        bool merged = false;
        for (lua_Integer index = 1; index <= count; ++index) {
            lua_rawgeti(state, round, index);
            const int entry = lua_gettop(state);
            lua_getfield(state, entry, "key");
            lua_pushvalue(state, -1);
            const bool mapped = lua_rawget(state, map) != LUA_TNIL;
            if (!mapped && !forced) {
                deferred.push(state);
                lua_pushvalue(state, entry);
                lua_rawseti(state, -2, static_cast<lua_Integer>(lua_rawlen(state, -2)) + 1);
                lua_settop(state, entry - 1);
                continue;
            }
            if (!mapped) {
                lua_pop(state, 1);
                lua_pushvalue(state, -1);
            }
            const int liveKey = lua_gettop(state);
            lua_getfield(state, entry, "live");
            lua_getfield(state, entry, "base");
            lua_getfield(state, entry, "fresh");
            lua_pushvalue(state, entry + 1);
            lua_rawget(state, -2);
            lua_getfield(state, entry, "field");
            mergeField(liveKey + 1, liveKey + 2, liveKey, liveKey + 4, lua_tostring(state, -1));
            merged = true;
            lua_settop(state, entry - 1);
        }
        forced = !merged;
        lua_pop(state, 1);
    }
    lua_settop(state, map - 1);
}

void ModuleReloader::mergeMetatables(int live, int fresh, const std::string& field) {
    const int top = lua_gettop(state);
    const bool liveHas = lua_getmetatable(state, live) != 0;
    const int liveMeta = lua_gettop(state);
    const bool freshHas = lua_getmetatable(state, fresh) != 0;
    const int freshMeta = lua_gettop(state);
    if (liveHas && freshHas && lua_rawequal(state, liveMeta, freshMeta) == 0 && ModuleSnapshot::isMergeable(state, liveMeta) && ModuleSnapshot::isMergeable(state, freshMeta)) {
        lua_pushnil(state);
        mergeTable(liveMeta, freshMeta, lua_gettop(state), field + " metatable");

        // The tables a merged metatable belongs to, such as the classes themselves, are no instances whose hooks run.
        patched.push(state);
        lua_pushvalue(state, liveMeta);
        lua_pushnil(state);
        lua_rawset(state, -3);
    } else if (!liveHas && freshHas) {
        lua_pushvalue(state, freshMeta);
        lua_setmetatable(state, live);
    }
    lua_settop(state, top);
}

void ModuleReloader::mapReplaced(int live, int base, int depth) {
    if (!lua_istable(state, base) || depth > 8) {
        return;
    }
    luaL_checkstack(state, 8, kDepthMessage);
    const int current = lua_absindex(state, live);
    const int previous = lua_absindex(state, base);
    lua_pushnil(state);
    while (lua_next(state, previous) != 0) {
        const int value = lua_gettop(state);
        lua_pushvalue(state, value - 1);
        lua_rawget(state, current);
        if (lua_isfunction(state, value) && lua_isfunction(state, -1)) {
            mapFunction(value, -1);
        } else if (lua_istable(state, value) && ModuleSnapshot::isMergeable(state, -1)) {
            mapReplaced(-1, value, depth + 1);
        }
        lua_settop(state, value - 1);
    }
}

// Only functions of the module file map, so a field that now holds a function of another module never replaces that function everywhere.
void ModuleReloader::mapFunction(int previous, int fresh) {
    const int old = lua_absindex(state, previous);
    const int next = lua_absindex(state, fresh);
    if (!lua_isfunction(state, old) || lua_rawequal(state, old, next) != 0 || !isOfModule(old)) {
        return;
    }
    replacements.push(state);
    lua_pushvalue(state, old);
    lua_pushvalue(state, next);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

bool ModuleReloader::isOfModule(int function) {
    if (lua_iscfunction(state, function) != 0) {
        return false;
    }
    lua_Debug info{};
    lua_pushvalue(state, function);
    lua_getinfo(state, ">S", &info);
    return info.source[0] == '@' && path == info.source + 1;
}

bool ModuleReloader::isClaimed(int index, bool byModule) {
    const int table = lua_absindex(state, index);
    ModuleGraph::pushCreators(state);
    lua_pushvalue(state, table);
    lua_rawget(state, -2);
    const bool claimed = byModule ? lua_type(state, -1) == LUA_TSTRING && path == lua_tostring(state, -1) : !lua_isnil(state, -1);
    lua_pop(state, 2);
    return claimed;
}

// The new functions take over the variables of the old functions with the same names, after the old variables took the merged values, so old closures, such as a coroutine suspended in old code, and new closures share the state of the module from now on.
void ModuleReloader::joinVariables(const ModuleClosures& previous, const ModuleClosures& fresh, int base) {
    luaL_checkstack(state, 8, kDepthMessage);
    const int baseIndex = lua_absindex(state, base);
    for (const auto& [variable, cells] : fresh.getCells()) {
        const auto old = previous.getCells().find(variable);
        if (old == previous.getCells().end()) {
            continue;
        }
        const ModuleClosures::Cell& target = old->second.front();
        const int mark = lua_gettop(state);
        previous.pushClosure(state, target);
        const int oldClosure = lua_gettop(state);
        lua_getupvalue(state, oldClosure, target.upvalue);
        fresh.pushClosure(state, cells.front());
        lua_getupvalue(state, -1, cells.front().upvalue);
        lua_remove(state, -2);
        if (lua_istable(state, baseIndex)) {
            lua_getfield(state, baseIndex, variable.c_str());
        } else {
            lua_pushnil(state);
        }
        pushMerged(mark + 4, mark + 2, mark + 3, "local " + variable);
        lua_setupvalue(state, oldClosure, target.upvalue);
        for (const ModuleClosures::Cell& cell : cells) {
            fresh.pushClosure(state, cell);
            lua_upvaluejoin(state, -1, cell.upvalue, oldClosure, target.upvalue);
            lua_pop(state, 1);
        }
        lua_settop(state, mark);
    }
}

void ModuleReloader::mergeGlobals(int staging, int base) {
    luaL_checkstack(state, 8, kDepthMessage);
    const int written = lua_absindex(state, staging);
    const int previous = lua_absindex(state, base);
    const bool hasBase = lua_istable(state, previous);
    lua_pushglobaltable(state);
    const int globals = lua_gettop(state);
    lua_pushnil(state);
    while (lua_next(state, written) != 0) {
        const int value = lua_gettop(state);
        const int key = value - 1;
        if (hasBase) {
            lua_pushvalue(state, key);
            lua_rawget(state, previous);
        } else {
            lua_pushnil(state);
        }
        lua_pushvalue(state, key);
        lua_rawget(state, globals);
        const std::string global = lua_type(state, key) == LUA_TSTRING ? std::string("global ") + lua_tostring(state, key) : std::string("global");
        pushMerged(value + 1, value + 2, value, global);
        lua_pushvalue(state, key);
        lua_insert(state, -2);
        lua_rawset(state, globals);
        lua_settop(state, key);
    }

    // A global that the new source no longer writes goes away, unless the app changed it since the last load.
    if (hasBase) {
        lua_pushnil(state);
        while (lua_next(state, previous) != 0) {
            const int value = lua_gettop(state);
            const int key = value - 1;
            lua_pushvalue(state, key);
            const bool absent = lua_rawget(state, written) == LUA_TNIL;
            lua_pushvalue(state, key);
            lua_rawget(state, globals);
            if (absent && ModuleSnapshot::equals(state, -1, value)) {
                lua_pushvalue(state, key);
                lua_pushnil(state);
                lua_rawset(state, globals);
            }
            lua_settop(state, key);
        }
    }
    lua_pop(state, 1);
}

void ModuleReloader::pushPreviousGlobals(int base) {
    const int previous = lua_absindex(state, base);
    lua_newtable(state);
    if (!lua_istable(state, previous)) {
        return;
    }
    const int result = lua_gettop(state);
    lua_pushglobaltable(state);
    lua_pushnil(state);
    while (lua_next(state, previous) != 0) {
        lua_pop(state, 1);
        lua_pushvalue(state, -1);
        lua_pushvalue(state, -1);
        lua_rawget(state, result + 1);
        lua_rawset(state, result);
    }
    lua_pop(state, 1);
}

} // namespace haylen::lua
