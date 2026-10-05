#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "lua/ModuleGraph.hpp"

namespace haylen::platform {
class DevelopmentSession;
}

namespace haylen::plugins {

// Applies the changes of an app in development, even while the error screen shows. Changed assets reload in place, a changed Lua module reloads in place with the state of the app in the mode `module` of `debug.reload`, and a change that cannot apply in place restarts the app: `app.json`, `source/main.lua`, a plugin manifest, a removed module, a module that cannot merge and any module in the mode `restart`. A batch that fixed an error of frame code or of a reload resumes the app from the error screen. The changes come from the development session of the host, from its scans of the package folder, the page or the development server. The plugin installs `haylen.hotReload` in every app, and the module graph only in development, so apps that ship load their modules exactly as before.
class HotReloadPlugin final : public Plugin {
  public:
    static constexpr float kScanSeconds = 0.5F;

    // A batch that touches more loaded modules than this, such as a switch of branches, restarts the app instead.
    static constexpr std::size_t kRestartModules = 32;

    // What one batch of changes did, for the log, the page and the development server.
    struct Report {
        core::AppConfig::Debug::Reload mode = core::AppConfig::Debug::Reload::Module;
        std::vector<std::string> modules;
        std::vector<std::string> assets;
        bool resumed = false;
        bool restarted = false;
        std::string reason;
        double milliseconds = 0.0;

        [[nodiscard]] core::Json toJson() const;
    };

    explicit HotReloadPlugin(platform::DevelopmentSession* developmentSession) noexcept : session(developmentSession) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "hotReload";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;

    // Whether the app runs in development, where changes to its files reach it.
    [[nodiscard]] bool isActive() const noexcept {
        return session != nullptr;
    }

    // The loaded modules of the app, which only an app in development records, or null.
    [[nodiscard]] lua::ModuleGraph* getGraph() noexcept {
        return graph.get();
    }

    // Fires after every batch that did something, with its report.
    core::Signal<const Report&> reloaded;

  private:
    // The paths of a batch by what they do.
    struct Batch {
        std::vector<std::string> assets;
        std::vector<std::string> modules;
        std::string restart;
    };

    [[nodiscard]] Batch classify(core::Engine& engine, const std::vector<std::string>& changed) const;
    void apply(core::Engine& engine, const std::vector<std::string>& changed);
    void reloadAssets(core::Engine& engine, const std::vector<std::string>& assets, Report& report) const;

    // Reloads the modules in dependency order and returns whether all of them applied, or leaves a restart reason in the report.
    [[nodiscard]] bool reloadModules(core::Engine& engine, const std::vector<std::string>& paths, Report& report);
    void callHooks(core::Engine& engine, const std::vector<std::string>& paths, int instances) const;
    void finish(const Report& report);

    platform::DevelopmentSession* session;
    std::unique_ptr<lua::ModuleGraph> graph;
    core::AppConfig::Debug::Reload mode = core::AppConfig::Debug::Reload::Module;
    float elapsed = 0.0F;
};

} // namespace haylen::plugins
