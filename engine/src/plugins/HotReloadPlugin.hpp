#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/AppError.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/plugins/Plugin.hpp"

namespace haylen::platform {
class DevelopmentSession;
}

namespace haylen::plugins {

// Applies the changes of an app in development, even while the error screen shows. Changed assets reload in place, a changed Lua module reloads in place with the state of the app, a removed module that no code of the app requires leaves the app in place, and a change that cannot apply in place restarts the app: `haylen.json`, `source/main.lua`, a plugin manifest, a removed module that code of the app still requires, a module that cannot merge and any module while `lua.reload` is `restart`. The report of every batch tells for each changed file whether it applied in place, waits for the next load, never counts, was rejected or restarts the app, and why. A batch that fixed an error of frame code or of a reload resumes the app from the error screen. The changes come from the development session of the host, from its scans of the package folder, the page or the development server. The runtime of the scripts reloads their code through `Scripts`, which the Lua module of hot reload gives the plugin only in development, so apps that ship load their modules exactly as before.
class HotReloadPlugin final : public Plugin {
  public:
    static constexpr std::chrono::milliseconds kScanInterval{500};

    // A batch that touches more loaded modules than this, such as a switch of branches, restarts the app instead.
    static constexpr std::size_t kRestartModules = 32;

    // What a batch did with a changed file: applied it in place, left it to the next load that reads it, left it out since hot reload never reads such a file, rejected it with an error, or restarts the app for it.
    enum class Change : std::uint8_t {
        InPlace,
        NextLoad,
        Ignored,
        Rejected,
        Restart,
    };

    static constexpr std::array<std::string_view, 5> kChangeNames{"inPlace", "nextLoad", "ignored", "rejected", "restart"};

    // A changed file with what the batch did with it and the sentence that tells why.
    struct File {
        std::string path;
        Change change = Change::InPlace;
        std::string reason;
    };

    // A changed file that the batch could not apply, with the error that kept it out.
    struct Rejection {
        std::string path;
        core::AppError error;
    };

    // What one batch of changes did, for the log, the page and the development server, with every changed file. The restored modules are those whose file holds again the code that runs, after a reload of it failed.
    struct Report {
        std::vector<File> files;
        std::vector<std::string> modules;
        std::vector<std::string> restored;
        std::vector<std::string> assets;
        std::vector<Rejection> rejected;
        bool resumed = false;
        bool restarted = false;
        std::string reason;
        double milliseconds = 0.0;

        [[nodiscard]] core::Json toJson() const;
    };

    // What reloads the code of the scripts of an app in development, such as the modules of Lua, which the runtime of the scripts gives the plugin while the app runs.
    class Scripts {
      public:
        virtual ~Scripts() = default;

        // Whether the app loaded the file as code.
        [[nodiscard]] virtual bool isLoaded(std::string_view path) const = 0;

        // The reason why a change of the file of loaded code restarts the app, or an empty text when it applies in place.
        [[nodiscard]] virtual std::string findRestartReason(core::Engine& engine, const std::string& path) const = 0;

        // Takes the code of a removed file that no code of the app requires out of the app.
        virtual void remove(core::Engine& engine, const std::string& path) = 0;

        // Reloads the code of the files in the order the code requires each other, writes what each file did into the report and returns whether all of them and their hooks applied. A file that cannot apply in place writes a restart with its reason into the report and names itself in `restartFile`.
        [[nodiscard]] virtual bool reload(core::Engine& engine, const std::vector<std::string>& paths, Report& report, std::string& restartFile) = 0;
    };

    explicit HotReloadPlugin(platform::DevelopmentSession* developmentSession) noexcept : session(developmentSession) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "hotReload";
    }
    void beginFrame(core::Engine& engine, float deltaSeconds) override;

    // Whether the app runs in development, where changes to its files reach it.
    [[nodiscard]] bool isActive() const noexcept {
        return session != nullptr;
    }

    // Gives the plugin what reloads the code of the scripts of the app while the app runs in development, or none.
    void setScripts(Scripts* value) noexcept;

    // Fires after every batch that did something, with its report.
    core::Signal<const Report&> reloaded;

  private:
    // The paths of a batch by what they do: the assets by their asset paths and the package paths of the same files, which plugins hear, the modules, and the removed modules that no code of the app requires. A batch that restarts the app keeps the reason, and the file that asked for it, if one did.
    struct Batch {
        std::vector<std::string> assets;
        std::vector<std::string> files;
        std::vector<std::string> modules;
        std::vector<std::string> removed;
        std::string restart;
        std::string restartFile;
        bool retry = false;
    };

    [[nodiscard]] Batch classify(core::Engine& engine, const std::vector<std::string>& changed, Report& report) const;

    // The reason why a changed file of `source/` or a plugin restarts the app, or an empty text when it does not.
    [[nodiscard]] std::string findRestartReason(core::Engine& engine, const std::string& path) const;
    void apply(core::Engine& engine, const std::vector<std::string>& changed);
    void reloadAssets(core::Engine& engine, const Batch& batch, Report& report) const;
    void removeModules(core::Engine& engine, const Batch& batch, Report& report);

    // Restarts the app for the reason, and marks every changed file that the batch would apply as applied by the restart.
    void restart(core::Engine& engine, const Batch& batch, Report& report, const std::string& reason, const std::string& file);

    void finish(const Report& report);

    platform::DevelopmentSession* session;
    Scripts* scripts = nullptr;
    std::chrono::steady_clock::time_point lastScan;
};

} // namespace haylen::plugins
