#include "lua/ModuleGraph.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <new>
#include <set>

#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/ModuleClosures.hpp"
#include "lua/ModuleSnapshot.hpp"
#include "lua/Owners.hpp"

namespace haylen::lua {

void ModuleGraph::install(lua_State* L) {
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &require, 1);
    lua_setglobal(L, "require");
    if (Userdata::newMetatable(L, kLoadType)) {
        lua_pushcfunction(L, &closeLoad);
        lua_setfield(L, -2, "__close");
    }
    lua_pop(L, 1);
}

const ModuleGraph::Module* ModuleGraph::find(std::string_view path) const {
    const auto found = modules.find(path);
    return found != modules.end() ? &found->second : nullptr;
}

ModuleGraph::Module* ModuleGraph::find(std::string_view path) {
    const auto found = modules.find(path);
    return found != modules.end() ? &found->second : nullptr;
}

std::vector<std::string> ModuleGraph::order(const std::vector<std::string>& paths) const {
    const std::set<std::string, std::less<>> wanted(paths.begin(), paths.end());
    std::set<std::string, std::less<>> visited;
    std::vector<std::string> ordered;
    // clang-format off
    const std::function<void(const std::string&)> visit = [&](const std::string& path) {
        if (!visited.insert(path).second) {
            return;
        }
        if (const Module* module = find(path)) {
            for (const std::string& required : module->dependencies) {
                visit(required);
            }
        }
        if (wanted.contains(path)) {
            ordered.push_back(path);
        }
    };
    // clang-format on
    for (const std::string& path : loadOrder) {
        if (wanted.contains(path)) {
            visit(path);
        }
    }
    return ordered;
}

std::vector<std::string> ModuleGraph::findDependents(const std::vector<std::string>& paths) const {
    std::set<std::string, std::less<>> reached(paths.begin(), paths.end());
    std::vector<std::string> dependents;
    for (bool grew = true; grew;) {
        grew = false;
        for (const std::string& path : loadOrder) {
            const Module& module = modules.at(path);
            if (reached.contains(path) || std::ranges::none_of(module.dependencies, [&reached](const std::string& required) { return reached.contains(required); })) {
                continue;
            }
            reached.insert(path);
            dependents.push_back(path);
            grew = true;
        }
    }
    std::ranges::sort(dependents, [this](const std::string& left, const std::string& right) { return std::ranges::find(loadOrder, left) < std::ranges::find(loadOrder, right); });
    return dependents;
}

std::string_view ModuleGraph::getLoading() const noexcept {
    return loading.empty() ? std::string_view{} : std::string_view(loading.back().path);
}

void ModuleGraph::beginLoad(lua_State* L, const std::string& path, int owner) {
    loading.push_back({.path = path, .owner = Reference(L, owner)});
    Owners::swapDefault(L, owner);
    lua_pop(L, 1);
}

// The owner of the module that loads below the one that ended takes registrations again, so nested requires keep their own owners.
void ModuleGraph::endLoad(lua_State* L) {
    loading.pop_back();
    if (loading.empty()) {
        lua_pushnil(L);
    } else {
        loading.back().owner.push(L);
    }
    Owners::swapDefault(L, -1);
    lua_pop(L, 2);
}

void ModuleGraph::pushStore(lua_State* L) {
    if (lua_getfield(L, LUA_REGISTRYINDEX, kStore) == LUA_TTABLE) {
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, kStore);
}

void ModuleGraph::pushCreators(lua_State* L) {
    pushStore(L);
    if (lua_getfield(L, -1, "creators") != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_createtable(L, 0, 1);
        lua_pushliteral(L, "k");
        lua_setfield(L, -2, "__mode");
        lua_setmetatable(L, -2);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "creators");
    }
    lua_remove(L, -2);
}

void ModuleGraph::pushSection(lua_State* L, const char* name) {
    pushStore(L);
    if (lua_getfield(L, -1, name) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, name);
    }
    lua_remove(L, -2);
}

void ModuleGraph::pushOwner(lua_State* L, std::string_view path) {
    pushSection(L, "owners");
    lua_pushlstring(L, path.data(), path.size());
    lua_rawget(L, -2);
    lua_remove(L, -2);
}

void ModuleGraph::pushSnapshot(lua_State* L, std::string_view path) {
    pushSection(L, "snapshots");
    lua_pushlstring(L, path.data(), path.size());
    lua_rawget(L, -2);
    lua_remove(L, -2);
}

void ModuleGraph::pushKept(lua_State* L, std::string_view path) {
    pushSection(L, "kept");
    lua_pushlstring(L, path.data(), path.size());
    if (lua_rawget(L, -2) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushlstring(L, path.data(), path.size());
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
    }
    lua_remove(L, -2);
}

void ModuleGraph::setOwner(lua_State* L, std::string_view path, int index) {
    const int value = lua_absindex(L, index);
    pushSection(L, "owners");
    lua_pushlstring(L, path.data(), path.size());
    lua_pushvalue(L, value);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

void ModuleGraph::setSnapshot(lua_State* L, std::string_view path, int index) {
    const int value = lua_absindex(L, index);
    pushSection(L, "snapshots");
    lua_pushlstring(L, path.data(), path.size());
    lua_pushvalue(L, value);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

// The `require` of development: modules already loaded come from `package.loaded` as always, and a module of the package runs under the record of the graph, while modules of `package.preload` load as they always do.
int ModuleGraph::require(lua_State* L) {
    auto& graph = *static_cast<ModuleGraph*>(lua_touserdata(L, lua_upvalueindex(1)));
    const std::string name = luaL_checkstring(L, 1);
    lua_settop(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_getfield(L, 2, name.c_str());
    if (lua_toboolean(L, -1) != 0) {
        graph.recordRequire(L, name);
        return 1;
    }
    lua_pop(L, 1);

    findLoader(L, name.c_str());
    if (lua_type(L, 3) == LUA_TFUNCTION && lua_iscfunction(L, 3) == 0 && lua_type(L, 4) == LUA_TSTRING && std::string_view(lua_tostring(L, 4)).ends_with(".lua")) {
        return graph.load(L, name, lua_tostring(L, 4));
    }

    lua_pushvalue(L, 3);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 4);
    lua_call(L, 2, 1);
    if (!lua_isnil(L, -1)) {
        lua_setfield(L, 2, name.c_str());
    } else {
        lua_pop(L, 1);
    }
    if (lua_getfield(L, 2, name.c_str()) == LUA_TNIL) {
        lua_pushboolean(L, 1);
        lua_copy(L, -1, -2);
        lua_setfield(L, 2, name.c_str());
    }
    lua_pushvalue(L, 4);
    return 2;
}

// Asks every searcher of `package.searchers` in order and raises the message of `require` when none finds the module.
void ModuleGraph::findLoader(lua_State* L, const char* name) {
    lua_getfield(L, 2, "package");
    if (lua_type(L, -1) != LUA_TTABLE || lua_getfield(L, -1, "searchers") != LUA_TTABLE) {
        luaL_error(L, "'package.searchers' must be a table");
    }
    const int searchers = lua_gettop(L);
    luaL_Buffer message;
    luaL_buffinit(L, &message);
    luaL_addstring(&message, "\n\t");
    for (int index = 1;; ++index) {
        if (lua_rawgeti(L, searchers, index) == LUA_TNIL) {
            lua_pop(L, 1);
            luaL_buffsub(&message, 2);
            luaL_pushresult(&message);
            luaL_error(L, "module '%s' not found:%s", name, lua_tostring(L, -1));
        }
        lua_pushstring(L, name);
        lua_call(L, 1, 2);
        if (lua_isfunction(L, -2) != 0) {
            break;
        }
        if (lua_isstring(L, -2) != 0) {
            lua_pop(L, 1);
            luaL_addvalue(&message);
            luaL_addstring(&message, "\n\t");
        } else {
            lua_pop(L, 2);
        }
    }

    // Only the loader and its data stay, at indices 3 and 4, without the package table, the searchers and the placeholder of the message buffer.
    lua_copy(L, -2, 3);
    lua_copy(L, -1, 4);
    lua_settop(L, 4);
}

// A module that a module requires, while it loads or later, is one it depends on. The caller is the function one level up, the one that called `require`.
void ModuleGraph::recordRequire(lua_State* L, const std::string& name) {
    lua_Debug caller{};
    if (lua_getstack(L, 1, &caller) == 0 || lua_getinfo(L, "S", &caller) == 0 || caller.source[0] != '@') {
        return;
    }
    const auto required = pathsByName.find(name);
    Module* module = find(caller.source + 1);
    if (required != pathsByName.end() && module != nullptr && required->second != module->path) {
        module->dependencies.insert(required->second);
    }
}

// Runs the top level of a module for the first time with a fresh owner and an environment that writes through to the globals while it records what the module writes, so its globals have a snapshot too. The load closes like a to-be-closed variable, so a module that fails leaves no record, no registrations and no recording environment behind.
int ModuleGraph::load(lua_State* L, const std::string& name, const std::string& path) {
    const bool first = !modules.contains(path);
    Module& module = modules[path];
    module.path = path;
    if (std::ranges::find(module.names, name) == module.names.end()) {
        module.names.push_back(name);
    }
    pathsByName[name] = path;
    if (first) {
        loadOrder.push_back(path);
    }
    recordRequire(L, name);

    lua_newtable(L);
    const int owner = lua_gettop(L);
    setOwner(L, path, owner);
    new (lua_newuserdatauv(L, sizeof(Load), 1)) Load{.graph = this};
    luaL_setmetatable(L, kLoadType);
    lua_pushvalue(L, 3);
    lua_setiuservalue(L, -2, 1);
    const int closing = lua_gettop(L);
    lua_toclose(L, closing);
    beginLoad(L, path, owner);

    lua_newtable(L);
    const int written = lua_gettop(L);
    lua_newtable(L);
    lua_createtable(L, 0, 2);
    lua_pushglobaltable(L);
    lua_setfield(L, -2, "__index");
    lua_pushvalue(L, written);
    lua_pushcclosure(L, &recordGlobal, 1);
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    setEnvironment(L, 3);

    lua_pushvalue(L, 3);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 4);
    lua_call(L, 2, 1);
    if (!lua_isnil(L, -1)) {
        lua_setfield(L, 2, name.c_str());
    } else {
        lua_pop(L, 1);
    }
    if (lua_getfield(L, 2, name.c_str()) == LUA_TNIL) {
        lua_pushboolean(L, 1);
        lua_copy(L, -1, -2);
        lua_setfield(L, 2, name.c_str());
    }
    const int result = lua_gettop(L);
    finishLoad(L, path, result, written);
    static_cast<Load*>(lua_touserdata(L, closing))->finished = true;
    lua_closeslot(L, closing);
    lua_pushvalue(L, result);
    lua_pushvalue(L, 4);
    return 2;
}

void ModuleGraph::finishLoad(lua_State* L, const std::string& path, int result, int written) {
    pushOwner(L, path);
    Owners::pushFunctions(L, -1);
    lua_remove(L, -2);
    const int functions = lua_gettop(L);
    const std::array<int, 3> roots{result, written, functions};
    const ModuleClosures closures(L, path, roots);
    closures.pushValues(L);
    const int upvalues = lua_gettop(L);
    pushCreators(L);
    ModuleSnapshot::push(L, result, upvalues, written, lua_gettop(L), path);
    setSnapshot(L, path, -1);
    lua_pop(L, 4);
    modules.at(path).source = Runtime::getEngine(L).getPackage().readText(path);
}

void ModuleGraph::setEnvironment(lua_State* L, int chunk) {
    const int function = lua_absindex(L, chunk);
    const char* name = lua_getupvalue(L, function, 1);
    if (name == nullptr) {
        lua_pop(L, 1);
        return;
    }
    lua_pop(L, 1);
    if (std::string_view(name) == "_ENV") {
        lua_setupvalue(L, function, 1);
    } else {
        lua_pop(L, 1);
    }
}

int ModuleGraph::recordGlobal(lua_State* L) {
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, lua_upvalueindex(1));
    lua_pushglobaltable(L);
    lua_insert(L, 2);
    lua_rawset(L, 2);
    return 0;
}

// Ends a first load. The chunk of the module reads the globals directly from now on, and a module whose top level failed is forgotten together with everything it registered.
int ModuleGraph::closeLoad(lua_State* L) {
    const auto& load = *static_cast<Load*>(lua_touserdata(L, 1));
    ModuleGraph& graph = *load.graph;
    const std::string path(graph.getLoading());

    lua_getiuservalue(L, 1, 1);
    lua_pushglobaltable(L);
    setEnvironment(L, -2);
    lua_pop(L, 1);
    graph.endLoad(L);
    if (load.finished) {
        return 0;
    }

    pushOwner(L, path);
    Owners::release(L, lua_gettop(L));
    lua_pop(L, 1);
    lua_pushnil(L);
    setOwner(L, path, -1);
    lua_pop(L, 1);
    const Module* module = graph.find(path);
    if (module != nullptr && module->source.empty()) {
        for (const std::string& name : module->names) {
            graph.pathsByName.erase(name);
        }
        std::erase(graph.loadOrder, path);
        graph.modules.erase(path);
    }
    return 0;
}

} // namespace haylen::lua
