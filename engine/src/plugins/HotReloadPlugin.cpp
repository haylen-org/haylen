#include "plugins/HotReloadPlugin.hpp"

#include <chrono>
#include <exception>
#include <optional>
#include <unordered_set>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/PackageWatcher.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "lua/Autoloads.hpp"
#include "lua/HotReloadLua.hpp"
#include "lua/ModuleReloader.hpp"
#include "lua/ScriptedScene.hpp"
#include "platform/DevelopmentSession.hpp"
#include "plugins/CorePlugin.hpp"

namespace haylen::plugins {

core::Json HotReloadPlugin::Report::toJson() const {
    if (restarted) {
        return {{"restarted", true}, {"reason", reason}};
    }
    return {{"restarted", false}, {"mode", core::AppConfig::reloadName(mode)}, {"modules", modules}, {"assets", assets}, {"resumed", resumed}, {"milliseconds", milliseconds}};
}

void HotReloadPlugin::start(core::Engine& engine) {
    mode = engine.getConfig().debug.reload;
}

// The module graph replaces `require` before the app loads any module, because plugins install their modules before the app starts.
void HotReloadPlugin::installLua(core::Engine&, lua_State* L) {
    lua::HotReloadLua::install(L);
    if (session != nullptr) {
        graph = std::make_unique<lua::ModuleGraph>();
        graph->install(L);
    }
}

// Frames never wait for a scan, and the next scan starts once the interval passed and the last one finished.
void HotReloadPlugin::beginFrame(core::Engine& engine, float) {
    if (session == nullptr) {
        return;
    }
    elapsed += static_cast<float>(engine.getClock().getUnscaledDelta());
    if (elapsed >= kScanSeconds) {
        if (const std::shared_ptr<platform::DevelopmentSession::Scan> scan = session->beginScan()) {
            elapsed = 0.0F;
            engine.getJobs().postIo([scan] { scan->run(); });
        }
    }

    const std::vector<std::string> changed = session->takeChanges();
    if (!changed.empty()) {
        apply(engine, changed);
    }
}

// A Lua file that no module loaded yet changes nothing, because the next `require` reads it. A batch restarts the app for `app.json`, `source/main.lua` and plugin manifests, for any loaded module in the mode `restart`, for more loaded modules than reload in place, and for any change while an error that cannot resume shows.
HotReloadPlugin::Batch HotReloadPlugin::classify(core::Engine& engine, const std::vector<std::string>& changed) const {
    Batch batch;
    for (const std::string& path : changed) {
        if (!io::PackageWatcher::isWatched(path)) {
            continue;
        }
        if (io::Path::isInside(path, io::Path::kContentDirectory)) {
            batch.assets.push_back(path.substr(io::Path::kContentDirectory.size() + 1));
            continue;
        }
        const bool manifest = path == io::Path::kAppConfigFile || path.ends_with(std::string("/") + std::string(io::Path::kPluginManifestFile));
        const bool main = path == std::string(io::Path::kSourceDirectory) + "/main.lua";
        const bool loaded = graph != nullptr && graph->find(path) != nullptr;
        if (manifest || main || (loaded && mode == core::AppConfig::Debug::Reload::Restart)) {
            batch.restart = "The file \"" + path + "\" changed, restarting the app.";
            return batch;
        }
        if (loaded) {
            batch.modules.push_back(path);
        }
    }

    const bool relevant = !batch.assets.empty() || !batch.modules.empty();
    if (relevant && engine.getError() != nullptr && !engine.isErrorResumable()) {
        batch.restart = "The app stopped in a step of its lifecycle, so the change restarts it.";
    } else if (batch.modules.size() > kRestartModules) {
        batch.restart = "The change touches " + std::to_string(batch.modules.size()) + " loaded modules, more than the " + std::to_string(kRestartModules) + " that reload in place, so the app restarts.";
    }
    return batch;
}

void HotReloadPlugin::apply(core::Engine& engine, const std::vector<std::string>& changed) {
    const auto started = std::chrono::steady_clock::now();
    Report report{.mode = mode};
    const Batch batch = classify(engine, changed);
    if (!batch.restart.empty()) {
        core::Log::info("{}", batch.restart);
        report.restarted = true;
        report.reason = batch.restart;
        engine.requestRestart();
        finish(report);
        return;
    }
    if (batch.assets.empty() && batch.modules.empty()) {
        return;
    }

    reloadAssets(engine, batch.assets, report);
    const bool applied = batch.modules.empty() || reloadModules(engine, batch.modules, report);
    if (report.restarted) {
        finish(report);
        return;
    }
    report.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    if (!report.modules.empty()) {
        std::string list;
        for (const std::string& path : report.modules) {
            list += (list.empty() ? "\"" : ", \"") + path + "\"";
        }
        core::Log::info("Reloaded {} in {:.0f} ms.", list, report.milliseconds);
    }
    if (applied && (!report.modules.empty() || !report.assets.empty()) && engine.clearError()) {
        report.resumed = true;
        core::Log::info("The app resumed after the reload.");
    }
    finish(report);
}

// An editor may still be writing a file when it is seen, so a failed reload waits for the next save instead of stopping the app.
void HotReloadPlugin::reloadAssets(core::Engine& engine, const std::vector<std::string>& assets, Report& report) const {
    for (const std::string& asset : assets) {
        try {
            if (engine.getAssets().reload(asset) > 0) {
                core::Log::info("Reloaded \"{}\".", asset);
                report.assets.push_back(asset);
            }
        } catch (const std::exception& error) {
            core::Log::warning("The asset \"{}\" could not be reloaded yet: {}", asset, error.what());
        }
    }
}

// A module that fails leaves the app as it was, and its error replaces the one the reload was meant to fix, so the error screen shows the latest one. The modules that reloaded before it keep their new code.
bool HotReloadPlugin::reloadModules(core::Engine& engine, const std::vector<std::string>& paths, Report& report) {
    lua_State* L = engine.getLuaState();
    const core::Engine::PhaseScope scope(engine, core::Engine::Phase::Reload);
    lua::ModuleReloader reloader(L, *graph, engine.getPackage());
    std::optional<lua::Error> failure;
    for (const std::string& path : graph->order(paths)) {
        lua::ModuleReloader::Result result = reloader.reload(path);
        if (result.status == lua::ModuleReloader::Status::Restart) {
            core::Log::info("{} The app restarts.", result.reason);
            report.restarted = true;
            report.reason = result.reason;
            engine.requestRestart();
            return false;
        }
        if (result.status == lua::ModuleReloader::Status::Failed) {
            failure = std::move(result.error);
            break;
        }
        if (result.status == lua::ModuleReloader::Status::Patched) {
            report.modules.push_back(path);
        }
    }

    const int top = lua_gettop(L);
    reloader.finish();
    if (!report.modules.empty()) {
        callHooks(engine, report.modules, top + 1);
        for (const std::string& path : report.modules) {
            engine.getEvents().post(std::string(core::LifecycleEvent::kModuleReloaded), {{"module", graph->find(path)->names.front()}, {"path", path}});
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

// Every table hears `reloaded` once, in this order: the reloaded modules and the modules that require them, the autoloads, the scenes from the bottom of the stack to the top, and the instances of the patched tables. Each call is protected like a scene hook.
void HotReloadPlugin::callHooks(core::Engine& engine, const std::vector<std::string>& paths, int instances) const {
    lua_State* L = engine.getLuaState();
    const int top = lua_gettop(L);
    lua_createtable(L, 0, 2);
    lua_createtable(L, static_cast<int>(paths.size()), 0);
    lua_createtable(L, static_cast<int>(paths.size()), 0);
    for (std::size_t index = 0; index < paths.size(); ++index) {
        lua_pushstring(L, graph->find(paths[index])->names.front().c_str());
        lua_rawseti(L, -3, static_cast<lua_Integer>(index + 1));
        lua_pushstring(L, paths[index].c_str());
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -3, "paths");
    lua_setfield(L, -2, "modules");
    const lua::Reference info(L, -1);

    std::unordered_set<const void*> called;
    // clang-format off
    const auto call = [&](int index) {
        if (!lua_istable(L, index) || !called.insert(lua_topointer(L, index)).second) {
            return;
        }
        const lua::Reference target(L, index);
        try {
            lua::Runtime::protectedRun(L, [&target, &info](lua_State* state) {
                target.push(state);
                lua_getfield(state, -1, "reloaded");
                if (!lua_isfunction(state, -1)) {
                    lua_pop(state, 2);
                    return;
                }
                lua_insert(state, -2);
                info.push(state);
                lua_call(state, 2, 0);
            });
        } catch (const std::exception& exception) {
            engine.reportError(exception);
        }
    };
    // clang-format on

    std::vector<std::string> modules = paths;
    const std::vector<std::string> dependents = graph->findDependents(paths);
    modules.insert(modules.end(), dependents.begin(), dependents.end());
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    for (const std::string& path : modules) {
        lua_getfield(L, -1, graph->find(path)->names.front().c_str());
        call(lua_gettop(L));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    engine.getPlugin<CorePlugin>().getAutoloads().pushList(L);
    for (lua_Integer index = 1; lua_rawgeti(L, -1, index) != LUA_TNIL; ++index) {
        call(lua_gettop(L));
        lua_pop(L, 1);
    }
    lua_pop(L, 2);

    const std::vector<std::shared_ptr<core::Scene>> scenes(engine.getScenes().begin(), engine.getScenes().end());
    for (const std::shared_ptr<core::Scene>& scene : scenes) {
        if (const auto* scripted = dynamic_cast<const lua::ScriptedScene*>(scene.get())) {
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
}

void HotReloadPlugin::finish(const Report& report) {
    reloaded.emit(report);
    session->report(report.toJson());
}

} // namespace haylen::plugins
