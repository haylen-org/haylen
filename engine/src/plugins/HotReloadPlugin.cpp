#include "plugins/HotReloadPlugin.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <optional>
#include <unordered_set>
#include <utility>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
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
#include "lua/ErrorReader.hpp"
#include "lua/HotReloadLua.hpp"
#include "lua/ModuleReloader.hpp"
#include "lua/ModuleSnapshot.hpp"
#include "lua/ScriptedScene.hpp"
#include "platform/DevelopmentSession.hpp"
#include "plugins/CorePlugin.hpp"

namespace haylen::plugins {

core::Json HotReloadPlugin::Report::toJson() const {
    core::Json changes = core::Json::array();
    for (const File& file : files) {
        changes.push_back({{"path", file.path}, {"change", kChangeNames[static_cast<std::size_t>(file.change)]}, {"reason", file.reason}});
    }

    if (restarted) {
        return {{"restarted", true}, {"reason", reason}, {"files", std::move(changes)}};
    }

    core::Json rejections = core::Json::array();
    for (const Rejection& rejection : rejected) {
        core::Json error = rejection.error.toJson();
        error["phase"] = core::Engine::kPhaseNames[static_cast<std::size_t>(core::Engine::Phase::Reload)];
        rejections.push_back({{"path", rejection.path}, {"error", std::move(error)}});
    }
    return {{"restarted", false}, {"mode", core::AppConfig::reloadName(mode)}, {"files", std::move(changes)}, {"modules", modules}, {"restored", restored}, {"assets", assets}, {"rejected", std::move(rejections)}, {"resumed", resumed}, {"milliseconds", milliseconds}};
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

// Frames never wait for a scan, and the next scan starts once the interval passed and the last one finished. The interval counts real time, because no time passes for a halted app, such as one that pauses while the editor has the focus.
void HotReloadPlugin::beginFrame(core::Engine& engine, float) {
    if (session == nullptr) {
        return;
    }

    session->pump();

    if (session->takeRestartRequest()) {
        const Report report{.restarted = true, .reason = "The developer restarted the app from the development server."};
        core::Log::info("{}", report.reason);
        engine.requestRestart();
        finish(report);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - lastScan >= kScanInterval) {
        if (const std::shared_ptr<platform::DevelopmentSession::Scan> scan = session->beginScan()) {
            lastScan = now;
            engine.getJobs().postIo([scan] { scan->run(); });
        }
    }

    // What a reload changed shows in the next frame, also in an app that redraws on change.
    const std::vector<std::string> changed = session->takeChanges();
    if (!changed.empty()) {
        apply(engine, changed);
        engine.requestRedraw();
    }
}

// A Lua file that no module loaded yet changes nothing, because the next `require` reads it, except while an error shows, since the file may hold the module whose first load failed: the batch then resumes the app, so the code that failed requires it again. Every file of the batch gets its entry in the report, and a batch restarts the app for `haylen.json`, `source/main.lua` and plugin manifests, for a removed module that code of the app still requires, for any loaded module in the mode `restart`, for more loaded modules than reload in place, and for any change while an error that cannot resume shows.
HotReloadPlugin::Batch HotReloadPlugin::classify(core::Engine& engine, const std::vector<std::string>& changed, Report& report) const {
    Batch batch;
    for (const std::string& path : changed) {
        if (!io::PackageWatcher::isWatched(path)) {
            report.files.push_back({.path = path, .change = Change::Ignored, .reason = "Hot reload never reads hidden files, backup files and files under \"source/\" that are not Lua."});
            continue;
        }

        if (io::Path::isInside(path, io::Path::kContentDirectory)) {
            batch.assets.push_back(path.substr(io::Path::kContentDirectory.size() + 1));
            batch.files.push_back(path);
            continue;
        }

        const std::string restartReason = findRestartReason(engine, path);
        if (!restartReason.empty()) {
            report.files.push_back({.path = path, .change = Change::Restart, .reason = restartReason});
            if (batch.restart.empty()) {
                batch.restart = restartReason;
                batch.restartFile = path;
            }

            continue;
        }

        const bool loaded = graph != nullptr && graph->find(path) != nullptr;
        if (loaded && !engine.getPackage().exists(path)) {
            batch.removed.push_back(path);
        } else if (loaded) {
            batch.modules.push_back(path);
        } else if (engine.getError() != nullptr) {
            batch.retry = true;
            report.files.push_back({.path = path, .change = Change::NextLoad, .reason = "No module of the app loaded the file, and the app resumes, so the code that failed requires it again."});
        } else {
            core::Log::info("The file \"{}\" changed, and no module of the app loaded it yet, so the next \"require\" reads it.", path);
            report.files.push_back({.path = path, .change = Change::NextLoad, .reason = "No module of the app loaded the file yet, so the next \"require\" reads it."});
        }
    }

    if (!batch.restart.empty()) {
        return batch;
    }

    const bool relevant = !batch.assets.empty() || !batch.modules.empty() || !batch.removed.empty() || batch.retry;
    if (relevant && engine.getError() != nullptr && !engine.isErrorResumable()) {
        batch.restart = "The app stopped in a step of its lifecycle, so the change restarts it.";
    } else if (batch.modules.size() > kRestartModules) {
        batch.restart = "The change touches " + std::to_string(batch.modules.size()) + " loaded modules, more than the " + std::to_string(kRestartModules) + " that reload in place, so the app restarts.";
    }

    return batch;
}

// A removed module leaves the app in place only while no code of the app requires it: no other module, no file that is no module, such as `source/main.lua`, and no autoload.
std::string HotReloadPlugin::findRestartReason(core::Engine& engine, const std::string& path) const {
    if (path == io::Path::kAppConfigFile) {
        return "The file \"haylen.json\" configures the app before any Lua runs, so the app restarts.";
    }

    if (path.ends_with(std::string("/") + std::string(io::Path::kPluginManifestFile))) {
        return "The plugin manifest \"" + path + "\" configures the app before any Lua runs, so the app restarts.";
    }

    if (path == std::string(io::Path::kSourceDirectory) + "/main.lua") {
        return "The file \"source/main.lua\" starts the app, so the app restarts.";
    }

    const lua::ModuleGraph::Module* module = graph != nullptr ? graph->find(path) : nullptr;
    if (module == nullptr) {
        return {};
    }

    if (mode == core::AppConfig::Debug::Reload::Restart) {
        return "The module \"" + module->names.front() + "\" changed while \"debug.reload\" of \"haylen.json\" is \"restart\", so the app restarts.";
    }

    if (engine.getPackage().exists(path)) {
        return {};
    }

    const std::vector<std::string> dependents = graph->findDependents({path});
    if (!dependents.empty()) {
        return "The module file \"" + path + "\" was removed, and \"" + dependents.front() + "\" requires it, so the app restarts.";
    }

    if (graph->isRequiredOutsideModules(path)) {
        return "The module file \"" + path + "\" was removed, and a file of the app that is no module, such as \"source/main.lua\", requires it, so the app restarts.";
    }

    const lua::Autoloads& autoloads = engine.getPlugin<CorePlugin>().getAutoloads();
    const bool autoload = std::ranges::any_of(module->names, [&autoloads](const std::string& name) { return autoloads.hasModule(name); });
    if (autoload) {
        return "The module file \"" + path + "\" was removed, and the app keeps it as an autoload, so the app restarts.";
    }

    return {};
}

void HotReloadPlugin::restart(core::Engine& engine, const Batch& batch, Report& report, const std::string& reason, const std::string& file) {
    const std::string applied = file.empty() ? reason : "The app restarts for \"" + file + "\", which applies this file too.";
    for (File& entry : report.files) {
        if (entry.change != Change::Ignored && entry.change != Change::Restart) {
            entry.change = Change::Restart;
            entry.reason = applied;
        }
    }

    for (const std::vector<std::string>* paths : {&batch.files, &batch.modules, &batch.removed}) {
        for (const std::string& path : *paths) {
            const bool listed = std::ranges::any_of(report.files, [&path](const File& entry) { return entry.path == path; });
            if (!listed) {
                report.files.push_back({.path = path, .change = Change::Restart, .reason = applied});
            }
        }
    }

    core::Log::info("{}", reason);
    report.restarted = true;
    report.reason = reason;
    engine.requestRestart();
    finish(report);
}

void HotReloadPlugin::apply(core::Engine& engine, const std::vector<std::string>& changed) {
    const auto started = std::chrono::steady_clock::now();
    Report report{.mode = mode};
    const Batch batch = classify(engine, changed, report);
    if (!batch.restart.empty()) {
        restart(engine, batch, report, batch.restart, batch.restartFile);
        return;
    }

    reloadAssets(engine, batch, report);

    for (Plugin* plugin : engine.getPlugins().getAll()) {
        plugin->packageChanged(engine, batch.files);
    }

    removeModules(engine, batch, report);
    std::string restartFile;
    const bool applied = batch.modules.empty() || reloadModules(engine, batch.modules, report, restartFile);

    if (report.restarted) {
        restart(engine, batch, report, report.reason, restartFile);
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

    for (const std::string& path : report.restored) {
        core::Log::info("The file \"{}\" holds the code that runs again.", path);
    }

    const bool changedCode = !report.modules.empty() || !report.restored.empty() || !report.assets.empty() || !batch.removed.empty() || batch.retry;
    if (applied && changedCode && engine.clearError()) {
        report.resumed = true;
        core::Log::info("The app resumed after the reload.");
    }

    finish(report);
}

// An editor may still be writing a file when it is seen, so a failed reload waits for the next save instead of stopping the app, and the report rejects the file with its reason. Assets of a type that cannot change in place leave the cache and announce `assetChanged`, so the next load reads the file.
void HotReloadPlugin::reloadAssets(core::Engine& engine, const Batch& batch, Report& report) const {
    for (std::size_t index = 0; index < batch.assets.size(); ++index) {
        const std::string& asset = batch.assets[index];
        const std::string& path = batch.files[index];
        try {
            const assets::Manager::Reload result = engine.getAssets().reload(asset);
            if (result.stale > 0) {
                report.files.push_back({.path = path, .change = Change::NextLoad, .reason = "Assets of a type that cannot change in place were read from the file, so they announced \"assetChanged\", and the next load reads the file."});
            } else if (result.inPlace > 0) {
                core::Log::info("Reloaded \"{}\".", asset);
                report.assets.push_back(asset);
                report.files.push_back({.path = path, .change = Change::InPlace, .reason = "The assets read from the file changed in place."});
            } else {
                report.files.push_back({.path = path, .change = Change::NextLoad, .reason = "No live asset was read from the file, so the next load reads it, and what the app built from it, such as its texts and themes, builds again."});
            }
        } catch (const std::exception& error) {
            core::Log::warning("The asset \"{}\" could not be reloaded yet: {}", asset, error.what());
            core::AppError rejection = lua::ErrorReader::read(std::string("The asset \"") + asset + "\" could not be reloaded: " + error.what());
            rejection.setCode(core::AppError::Code::Asset);
            report.files.push_back({.path = path, .change = Change::Rejected, .reason = rejection.getMessage()});
            report.rejected.push_back({.path = path, .error = std::move(rejection)});
        }
    }
}

// A removed module that no code of the app requires leaves `package.loaded`, and what it registered ends with it, so the next `require` of it fails as for a module that never existed.
void HotReloadPlugin::removeModules(core::Engine& engine, const Batch& batch, Report& report) {
    for (const std::string& path : batch.removed) {
        graph->remove(engine.getLuaState(), path);
        core::Log::info("The module file \"{}\" was removed, and no code of the app requires it, so it left the app.", path);
        report.files.push_back({.path = path, .change = Change::InPlace, .reason = "The module file was removed, and no code of the app requires it, so the module left the app."});
    }
}

// A module that fails, or whose file cannot be read, changes nothing, while the other modules of the batch still reload. Once the reload finished what it started, the first error of a module or of a hook replaces the one the reload was meant to fix, so the error screen shows the latest one.
bool HotReloadPlugin::reloadModules(core::Engine& engine, const std::vector<std::string>& paths, Report& report, std::string& restartFile) {
    lua_State* L = engine.getLuaState();
    const core::Engine::PhaseScope scope(engine, core::Engine::Phase::Reload);
    const int top = lua_gettop(L);
    lua::ModuleReloader reloader(L, *graph, engine.getPackage());
    std::optional<core::AppError> failure;
    for (const std::string& path : graph->order(paths)) {
        lua::ModuleReloader::Result result;
        try {
            result = reloader.reload(path);
        } catch (const std::exception& exception) {
            lua_settop(L, top);
            result = {.status = lua::ModuleReloader::Status::Failed, .error = lua::ErrorReader::read(exception.what())};
        }

        if (result.status == lua::ModuleReloader::Status::Restart) {
            const std::string reason = result.reason + " The app restarts.";
            report.files.push_back({.path = path, .change = Change::Restart, .reason = reason});
            report.restarted = true;
            report.reason = reason;
            restartFile = path;
            return false;
        }

        lua::ModuleGraph::Module& module = *graph->find(path);
        if (result.status == lua::ModuleReloader::Status::Failed) {
            module.failed = true;
            report.files.push_back({.path = path, .change = Change::Rejected, .reason = result.error->getMessage()});
            report.rejected.push_back({.path = path, .error = *result.error});
            if (!failure) {
                failure = std::move(result.error);
            }
        }

        if (result.status == lua::ModuleReloader::Status::Patched) {
            module.failed = false;
            report.modules.push_back(path);
            report.files.push_back({.path = path, .change = Change::InPlace, .reason = "The module reloaded in place, and the app keeps its state."});
        }

        if (result.status == lua::ModuleReloader::Status::Unchanged && std::exchange(module.failed, false)) {
            report.restored.push_back(path);
            report.files.push_back({.path = path, .change = Change::InPlace, .reason = "The file holds again the code that runs, after a reload of it failed."});
        } else if (result.status == lua::ModuleReloader::Status::Unchanged) {
            report.files.push_back({.path = path, .change = Change::InPlace, .reason = "The file holds the code that runs."});
        }
    }

    reloader.finish();

    if (!report.modules.empty()) {
        std::optional<core::AppError> hookFailure = callHooks(engine, report.modules, top + 1);
        if (!failure) {
            failure = std::move(hookFailure);
        }

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

// Every table hears `reloaded` once, in this order: the reloaded modules and the modules that require them, the autoloads, the scenes from the bottom of the stack to the top, and the instances of the patched tables. A module that returns a prototype, such as a class of `haylen.class`, defines the hook of its instances, so it is no module hook. Each call is protected like a scene hook.
std::optional<core::AppError> HotReloadPlugin::callHooks(core::Engine& engine, const std::vector<std::string>& paths, int instances) const {
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
    std::optional<core::AppError> failure;
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
        } catch (const core::AppError& error) {
            if (!failure) {
                failure = error;
            }
        }
    };
    // clang-format on

    std::vector<std::string> modules = paths;
    const std::vector<std::string> dependents = graph->findDependents(paths);
    modules.insert(modules.end(), dependents.begin(), dependents.end());
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    for (const std::string& path : modules) {
        lua_getfield(L, -1, graph->find(path)->names.front().c_str());
        if (!lua::ModuleSnapshot::isPrototype(L, -1)) {
            call(lua_gettop(L));
        }

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
    return failure;
}

void HotReloadPlugin::finish(const Report& report) {
    reloaded.emit(report);
    session->report(report.toJson());
}

} // namespace haylen::plugins
