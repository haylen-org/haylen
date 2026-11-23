#pragma once

#include <memory>
#include <string_view>

#include "haylen/lua/Modules.hpp"
#include "haylen/plugins/Plugin.hpp"

struct lua_State;

namespace varn::runtime {
class Runtime;
}

namespace haylen::platform {
class DevelopmentSession;
}

namespace haylen::lua {

class Autoloads;
class Jobs;
class Warnings;

// The engine plugin of the Lua layer, which an app that runs Lua adds after the plugins of the engine. Its start creates the Lua runtime of Varn on the scheduler of the engine, prepares the Lua state for the app, installs every Lua module in order and takes the modules that reach native code out of the state of an app without native access. It runs the jobs of `haylen.jobs` and the autoloads of the app, writes the warnings of Lua to the log, and closes the Lua state when the engine lets go of it, after every service of the engine released the Lua values it held.
class ScriptPlugin final : public plugins::Plugin {
  public:
    ScriptPlugin();
    ~ScriptPlugin() override;

    ScriptPlugin(const ScriptPlugin&) = delete;
    ScriptPlugin& operator=(const ScriptPlugin&) = delete;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "script";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void event(core::Engine& engine, const platform::Event& event) override;
    void fixedUpdate(core::Engine& engine, float stepSeconds) override;
    void update(core::Engine& engine, float deltaSeconds) override;
    void render(core::Engine& engine) override;
    void renderUi(core::Engine& engine, const core::SceneView& view) override;

    // Whether jobs wait for the next frame to go on.
    [[nodiscard]] bool isAnimating() const override;

    // Adds a Lua module after the modules of the engine, such as the module of a plugin of a project that extends the engine, and installs it at once when the Lua state already runs. Throws `std::invalid_argument` when a module of the same name is already added.
    template <typename T> T& addModule(std::unique_ptr<T> module) {
        T& added = modules.add(std::move(module));
        if (owner != nullptr) {
            install(added);
        }

        return added;
    }

    // Throws `std::logic_error` when no module of the type is added.
    template <typename T> [[nodiscard]] T& getModule() const {
        return modules.get<T>();
    }

    // The Lua state of the app, which exists from the start of the plugin until the engine lets go of it.
    [[nodiscard]] lua_State* getState() const noexcept;
    [[nodiscard]] varn::runtime::Runtime& getRuntime() noexcept;
    [[nodiscard]] Autoloads& getAutoloads() noexcept;
    [[nodiscard]] Jobs& getJobs() noexcept;

  private:
    void install(Module& module);
    void attachBudget(core::Engine& engine, lua_State* L);
    static int refuseHook(lua_State* L);

    core::Engine* owner = nullptr;
    platform::DevelopmentSession* session = nullptr;

    // The warnings outlive the Lua state that writes them, and the modules let go of what they hold before the state closes.
    std::unique_ptr<Warnings> warnings;
    std::unique_ptr<varn::runtime::Runtime> runtime;
    Modules modules;
    std::unique_ptr<Autoloads> autoloads;
    std::unique_ptr<Jobs> jobs;
};

} // namespace haylen::lua
