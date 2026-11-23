#include "development/ModuleHotReload.hpp"

#include <lua.hpp>

#include <algorithm>
#include <exception>
#include <memory>
#include <unordered_set>
#include <utility>

#include "development/ModuleReloader.hpp"
#include "development/ModuleSnapshot.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "runtime/Autoloads.hpp"
#include "runtime/ErrorReader.hpp"
#include "runtime/LuaConfig.hpp"
#include "runtime/ScriptedScene.hpp"

namespace haylen::lua {

// The module graph replaces `require` before the app loads any module, because the script plugin installs the modules before the app starts.
ModuleHotReload::ModuleHotReload(lua_State* L) : state(L) {
    graph.install(L);
}

bool ModuleHotReload::isLoaded(std::string_view path) const {
    return graph.find(path) != nullptr;
}

// A removed module leaves the app in place only while no code of the app requires it: no other module, no file that is no module, such as `source/main.lua`, and no autoload.
std::string ModuleHotReload::findRestartReason(haylen::core::Engine& engine, const std::string& path) const {
    const ModuleGraph::Module* module = graph.find(path);
    if (module == nullptr) {
        return {};
    }

    if (engine.getPlugin<ScriptPlugin>().getConfig().reload == LuaConfig::Reload::Restart) {
        return "The module \"" + module->names.front() + "\" changed while \"lua.reload\" of \"haylen.json\" is \"restart\", so the app restarts.";
    }

    if (engine.getPackage().exists(path)) {
        return {};
    }

    const std::vector<std::string> dependents = graph.findDependents({path});
    if (!dependents.empty()) {
        return "The module file \"" + path + "\" was removed, and \"" + dependents.front() + "\" requires it, so the app restarts.";
    }

    if (graph.isRequiredOutsideModules(path)) {
        return "The module file \"" + path + "\" was removed, and a file of the app that is no module, such as \"source/main.lua\", requires it, so the app restarts.";
    }

    const Autoloads& autoloads = engine.getPlugin<ScriptPlugin>().getAutoloads();
    const bool autoload = std::ranges::any_of(module->names, [&autoloads](const std::string& name) { return autoloads.hasModule(name); });
    if (autoload) {
        return "The module file \"" + path + "\" was removed, and the app keeps it as an autoload, so the app restarts.";
    }

    return {};
}

// A removed module that no code of the app requires leaves `package.loaded`, and what it registered ends with it, so the next `require` of it fails as for a module that never existed.
void ModuleHotReload::remove(haylen::core::Engine&, const std::string& path) {
    graph.remove(state, path);
}

// A module that fails, or whose file cannot be read, changes nothing, while the other modules of the batch still reload. Once the reload finished what it started, the first error of a module or of a hook replaces the one the reload was meant to fix, so the error screen shows the latest one.
bool ModuleHotReload::reload(haylen::core::Engine& engine, const std::vector<std::string>& paths, haylen::plugins::HotReloadPlugin::Report& report, std::string& restartFile) {
    using Change = haylen::plugins::HotReloadPlugin::Change;
    lua_State* L = state;
    const haylen::core::Engine::PhaseScope scope(engine, haylen::core::Engine::Phase::Reload);
    const int top = lua_gettop(L);
    ModuleReloader reloader(L, graph, engine.getPackage());
    std::optional<haylen::core::AppError> failure;
    for (const std::string& path : graph.order(paths)) {
        ModuleReloader::Result result;
        try {
            result = reloader.reload(path);
        } catch (const std::exception& exception) {
            lua_settop(L, top);
            result = {.status = ModuleReloader::Status::Failed, .error = ErrorReader::read(exception.what())};
        }

        if (result.status == ModuleReloader::Status::Restart) {
            const std::string reason = result.reason + " The app restarts.";
            report.files.push_back({.path = path, .change = Change::Restart, .reason = reason});
            report.restarted = true;
            report.reason = reason;
            restartFile = path;
            return false;
        }

        ModuleGraph::Module& module = *graph.find(path);
        if (result.status == ModuleReloader::Status::Failed) {
            module.failed = true;
            report.files.push_back({.path = path, .change = Change::Rejected, .reason = result.error->getMessage()});
            report.rejected.push_back({.path = path, .error = *result.error});
            if (!failure) {
                failure = std::move(result.error);
            }
        }

        if (result.status == ModuleReloader::Status::Patched) {
            module.failed = false;
            report.modules.push_back(path);
            report.files.push_back({.path = path, .change = Change::InPlace, .reason = "The module reloaded in place, and the app keeps its state."});
        }

        if (result.status == ModuleReloader::Status::Unchanged && std::exchange(module.failed, false)) {
            report.restored.push_back(path);
            report.files.push_back({.path = path, .change = Change::InPlace, .reason = "The file holds again the code that runs, after a reload of it failed."});
        } else if (result.status == ModuleReloader::Status::Unchanged) {
            report.files.push_back({.path = path, .change = Change::InPlace, .reason = "The file holds the code that runs."});
        }
    }

    reloader.finish();

    if (!report.modules.empty()) {
        std::optional<haylen::core::AppError> hookFailure = callHooks(engine, report.modules, top + 1);
        if (!failure) {
            failure = std::move(hookFailure);
        }

        for (const std::string& path : report.modules) {
            engine.getEvents().post(std::string(haylen::core::LifecycleEvent::kModuleReloaded), {{"module", graph.find(path)->names.front()}, {"path", path}});
        }
    }

    lua_settop(L, top);

    if (failure) {
        engine.clearError();
        engine.reportError(*failure);
        return false;
    }

    return true;
}

// Every table hears `reloaded` once, in this order: the reloaded modules and the modules that require them, the autoloads, the scenes from the bottom of the stack to the top, and the instances of the patched tables. A module that returns a prototype, such as a class of `haylen.class`, defines the hook of its instances, so it is no module hook. Each call is protected like a scene hook.
std::optional<haylen::core::AppError> ModuleHotReload::callHooks(haylen::core::Engine& engine, const std::vector<std::string>& paths, int instances) const {
    lua_State* L = state;
    const int top = lua_gettop(L);
    lua_createtable(L, 0, 2);
    lua_createtable(L, static_cast<int>(paths.size()), 0);
    lua_createtable(L, static_cast<int>(paths.size()), 0);
    for (std::size_t index = 0; index < paths.size(); ++index) {
        lua_pushstring(L, graph.find(paths[index])->names.front().c_str());
        lua_rawseti(L, -3, static_cast<lua_Integer>(index + 1));
        lua_pushstring(L, paths[index].c_str());
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    lua_setfield(L, -3, "paths");
    lua_setfield(L, -2, "modules");
    const Reference info(L, -1);

    std::unordered_set<const void*> called;
    std::optional<haylen::core::AppError> failure;
    // clang-format off
    const auto call = [&](int index) {
        if (!lua_istable(L, index) || !called.insert(lua_topointer(L, index)).second) {
            return;
        }

        const Reference target(L, index);
        try {
            Runtime::protectedRun(L, [&target, &info](lua_State* thread) {
                target.push(thread);
                lua_getfield(thread, -1, "reloaded");

                if (!lua_isfunction(thread, -1)) {
                    lua_pop(thread, 2);
                    return;
                }

                lua_insert(thread, -2);
                info.push(thread);
                lua_call(thread, 2, 0);
            });
        } catch (const haylen::core::AppError& error) {
            if (!failure) {
                failure = error;
            }
        }
    };
    // clang-format on

    std::vector<std::string> modules = paths;
    const std::vector<std::string> dependents = graph.findDependents(paths);
    modules.insert(modules.end(), dependents.begin(), dependents.end());
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    for (const std::string& path : modules) {
        lua_getfield(L, -1, graph.find(path)->names.front().c_str());
        if (!ModuleSnapshot::isPrototype(L, -1)) {
            call(lua_gettop(L));
        }

        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    engine.getPlugin<ScriptPlugin>().getAutoloads().pushList(L);
    for (lua_Integer index = 1; lua_rawgeti(L, -1, index) != LUA_TNIL; ++index) {
        call(lua_gettop(L));
        lua_pop(L, 1);
    }

    lua_pop(L, 2);

    const std::vector<std::shared_ptr<haylen::core::Scene>> scenes(engine.getScenes().begin(), engine.getScenes().end());
    for (const std::shared_ptr<haylen::core::Scene>& scene : scenes) {
        if (const auto* scripted = dynamic_cast<const ScriptedScene*>(scene.get())) {
            scripted->pushTable(L);
            call(lua_gettop(L));
            lua_pop(L, 1);
        }
    }

    for (lua_Integer index = 1; lua_rawgeti(L, instances, index) != LUA_TNIL; ++index) {
        call(lua_gettop(L));
        lua_pop(L, 1);
    }

    lua_settop(L, top);
    return failure;
}

} // namespace haylen::lua
