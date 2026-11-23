#include "development/ModuleGraph.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <set>
#include <utility>

#include "development/ModuleClosures.hpp"
#include "development/ModuleSnapshot.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "lua/Owners.hpp"

namespace haylen::lua {

void ModuleGraph::install(lua_State* L) {
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, &Binding::native<&require>, 1);
    lua_setglobal(L, "require");
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
    Owners::swapDefault(L, owner);
    loading.push_back({.path = path, .previous = Reference(L, -1)});
    lua_pop(L, 1);
}

// The owner that the run replaced takes registrations again, so nested requires and the code that runs under an owner of its own keep their owners.
void ModuleGraph::endLoad(lua_State* L, bool succeeded) {
    const Loading run = std::move(loading.back());
    loading.pop_back();

    if (!succeeded) {
        dropKept(L, run.path, run.kept);
    }

    run.previous.push(L);
    Owners::swapDefault(L, -1);
    lua_pop(L, 2);
}

// What `create` registers without an owner belongs to an owner of the kept value, which the store holds as long as the value, and a `create` that fails ends what it registered.
void ModuleGraph::pushKept(lua_State* L, const std::string& key, int create) {
    const int function = lua_absindex(L, create);
    const std::string path = loading.back().path;
    pushModuleTable(L, "kept", path);
    const int values = lua_gettop(L);
    if (lua_getfield(L, values, key.c_str()) != LUA_TNIL) {
        lua_remove(L, values);
        return;
    }

    lua_pop(L, 1);

    lua_newtable(L);
    const int owner = lua_gettop(L);
    lua_pushvalue(L, function);

    try {
        const Owners::DefaultScope scope(L, owner);
        Runtime::protectedCall(L, 0, 1);
    } catch (const core::AppError&) {
        Owners::release(L, owner);
        throw;
    }

    lua_pushvalue(L, -1);
    lua_setfield(L, values, key.c_str());
    pushModuleTable(L, "keptOwners", path);
    lua_pushvalue(L, owner);
    lua_setfield(L, -2, key.c_str());
    lua_pop(L, 1);

    // The function may have required modules, whose runs ended before it returned.
    loading.back().kept.push_back(key);
    lua_replace(L, values);
    lua_settop(L, values);
}

// A run that failed changes nothing, so the values it kept first go away and what their `create` registered ends with them.
void ModuleGraph::dropKept(lua_State* L, std::string_view path, const std::vector<std::string>& keys) {
    if (keys.empty()) {
        return;
    }

    pushModuleTable(L, "kept", path);
    pushModuleTable(L, "keptOwners", path);
    for (const std::string& key : keys) {
        lua_getfield(L, -1, key.c_str());
        Owners::release(L, lua_gettop(L));
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_setfield(L, -2, key.c_str());
        lua_pushnil(L);
        lua_setfield(L, -3, key.c_str());
    }

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

void ModuleGraph::pushModuleTable(lua_State* L, const char* section, std::string_view path) {
    pushSection(L, section);
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

// Asks every searcher of `package.searchers` in order and raises the error of the `require` of Lua, which lists what each searcher reported, when none finds the module, so a missing module reads the same in development and in a shipped app.
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
    if (required == pathsByName.end()) {
        return;
    }

    Module* module = find(caller.source + 1);
    if (module == nullptr) {
        requiredOutsideModules.insert(required->second);
        return;
    }

    if (required->second != module->path) {
        module->dependencies.insert(required->second);
    }
}

bool ModuleGraph::isRequiredOutsideModules(std::string_view path) const {
    return requiredOutsideModules.contains(path);
}

void ModuleGraph::remove(lua_State* L, const std::string& path) {
    const auto found = modules.find(path);
    if (found == modules.end()) {
        return;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    for (const std::string& name : found->second.names) {
        lua_pushnil(L);
        lua_setfield(L, -2, name.c_str());
        pathsByName.erase(name);
    }

    lua_pop(L, 1);

    // What the top level registered without an owner ends with the owner of the module, and what each kept value registered with the owner of that value.
    pushOwner(L, path);
    Owners::release(L, -1);
    lua_pop(L, 1);
    pushModuleTable(L, "keptOwners", path);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        Owners::release(L, -1);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    for (const char* section : {"owners", "snapshots", "kept", "keptOwners"}) {
        pushSection(L, section);
        lua_pushlstring(L, path.data(), path.size());
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }

    std::erase(loadOrder, path);
    requiredOutsideModules.erase(path);
    modules.erase(found);
}

// Runs the top level of a module for the first time with a fresh owner and an environment that writes through to the globals while it records what the module writes, so its globals have a snapshot too. Each step runs in a protected call, so a load that fails, in a coroutine too, leaves no record, no registrations and no recording environment behind at once. The top level runs in a kept call, so its error rises again unchanged with the stack of where it rose. The table `package.loaded` takes the module only once the graph recorded it. The text is read before the module runs, so a save while its top level runs reaches the next reload.
int ModuleGraph::load(lua_State* L, const std::string& name, const std::string& path) {
    FirstLoad run{.graph = this, .path = &path, .source = Runtime::getEngine(L).getPackage().readText(path)};
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
    const std::size_t depth = loading.size();
    lua_pushcfunction(L, &Binding::native<&beginFirstLoad>);
    lua_pushlightuserdata(L, &run);
    lua_pushvalue(L, 3);
    lua_pushvalue(L, owner);
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        abandonFirstLoad(L, path, 3, owner, depth);
        return lua_error(L);
    }

    const int written = lua_gettop(L);
    lua_pushvalue(L, 3);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 4);
    const int status = Runtime::keptCall(L, 2, 1);
    if (status != LUA_OK) {
        abandonFirstLoad(L, path, 3, owner, depth);
        return Runtime::raiseKept(L, status);
    }

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        if (lua_getfield(L, 2, name.c_str()) == LUA_TNIL) {
            lua_pop(L, 1);
            lua_pushboolean(L, 1);
        }
    }

    const int result = lua_gettop(L);
    lua_pushcfunction(L, &Binding::native<&finishFirstLoad>);
    lua_pushlightuserdata(L, &run);
    lua_pushvalue(L, 3);
    lua_pushvalue(L, result);
    lua_pushvalue(L, written);
    if (lua_pcall(L, 4, 0, 0) != LUA_OK) {
        abandonFirstLoad(L, path, 3, owner, depth);
        return lua_error(L);
    }

    lua_pushvalue(L, result);
    lua_setfield(L, 2, name.c_str());
    lua_pushvalue(L, result);
    lua_pushvalue(L, 4);
    return 2;
}

// Receives the first load, the chunk and the owner of the run, and returns the table that records the globals the module writes.
int ModuleGraph::beginFirstLoad(lua_State* L) {
    const FirstLoad& run = *static_cast<const FirstLoad*>(lua_touserdata(L, 1));
    setOwner(L, *run.path, 3);
    run.graph->beginLoad(L, *run.path, 3);
    pushRecordingEnvironment(L, 2);
    return 1;
}

// Receives the first load, the chunk, what the module produced and the globals it wrote. The chunk of the module reads the globals directly from then on.
int ModuleGraph::finishFirstLoad(lua_State* L) {
    FirstLoad& run = *static_cast<FirstLoad*>(lua_touserdata(L, 1));
    run.graph->finishLoad(L, *run.path, std::move(run.source), 3, 4);
    lua_pushglobaltable(L);
    setEnvironment(L, 2);
    run.graph->endLoad(L, true);
    return 0;
}

// Undoes a first load that failed at any of its steps: the chunk reads the globals again, the run ends, what the module registered ends, and a module that never loaded is forgotten.
void ModuleGraph::abandonFirstLoad(lua_State* L, const std::string& path, int chunk, int owner, std::size_t depth) {
    lua_pushglobaltable(L);
    setEnvironment(L, chunk);

    if (loading.size() > depth) {
        endLoad(L, false);
    }

    Owners::release(L, owner);
    lua_pushnil(L);
    setOwner(L, path, -1);
    lua_pop(L, 1);

    const Module* module = find(path);
    if (module == nullptr || !module->source.empty()) {
        return;
    }

    for (const std::string& name : module->names) {
        pathsByName.erase(name);
    }

    std::erase(loadOrder, path);
    modules.erase(path);
}

int ModuleGraph::pushRecordingEnvironment(lua_State* L, int chunk) {
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
    setEnvironment(L, chunk);
    return written;
}

void ModuleGraph::finishLoad(lua_State* L, const std::string& path, std::string source, int result, int written) {
    pushOwner(L, path);
    Owners::pushFunctions(L, -1);
    lua_remove(L, -2);
    const int functions = lua_gettop(L);
    const std::array<int, 3> roots{result, written, functions};
    const ModuleClosures closures(L, path, roots);
    closures.pushValues(L);
    const int upvalues = lua_gettop(L);
    pushCreators(L);
    ModuleSnapshot::push(L, result, upvalues, written, lua_gettop(L), path, 0);
    setSnapshot(L, path, -1);
    lua_pop(L, 4);
    modules.at(path).source = std::move(source);
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

} // namespace haylen::lua
