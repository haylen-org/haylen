#pragma once

#include <any>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneLoad.hpp"
#include "haylen/lua/Error.hpp"

namespace haylen::test {

// Scene that writes every hook it receives to a shared log as `name:hook`. Its load finishes when the hook returns, waits for a deferral that the test completes, or fails, and it can preload asset groups.
class RecordingScene final : public core::Scene {
  public:
    enum class Load {
        Instant,
        Deferred,
        Failing,
    };

    RecordingScene(std::string label, std::vector<std::string>& entries, bool see = false, core::ProcessMode processMode = core::ProcessMode::Inherit) : name(std::move(label)), log(entries), transparent(see), mode(processMode) {}

    void load(core::Engine&, core::SceneLoad& context) override {
        record("load");
        loadParams = context.getParams();
        for (const std::string& group : groups) {
            context.preload(group);
        }
        if (loading == Load::Deferred) {
            deferral.emplace(context.defer());
        }
        if (loading == Load::Failing) {
            throw lua::Error(name + " cannot load");
        }
    }
    void enter(core::Engine&, const std::any& params) override {
        enterParams = params;
        record("enter");
    }
    void enterTransitionFinished(core::Engine&) override {
        record("enterTransitionFinished");
    }
    void exitTransitionStarted(core::Engine&) override {
        record("exitTransitionStarted");
    }
    void exit(core::Engine&) override {
        record("exit");
    }
    void unload(core::Engine&) override {
        record("unload");
    }
    void pause(core::Engine&) override {
        record("pause");
    }
    void resume(core::Engine&) override {
        record("resume");
    }
    void paused(core::Engine&) override {
        record("paused");
    }
    void unpaused(core::Engine&) override {
        record("unpaused");
    }
    void event(core::Engine&, const platform::Event&) override {
        record("event");
    }
    void fixedUpdate(core::Engine&, float) override {
        ++fixedSteps;
    }
    void update(core::Engine&, float) override {
        ++updates;
    }
    void render(core::Engine&) override {
        ++renders;
    }
    void renderUi(core::Engine&) override {
        ++uiRenders;
    }
    [[nodiscard]] bool isTransparent() const noexcept override {
        return transparent;
    }
    [[nodiscard]] core::ProcessMode getProcessMode() const override {
        return mode;
    }
    [[nodiscard]] std::string getName() const override {
        return name;
    }

    Load loading = Load::Instant;
    std::vector<std::string> groups;
    std::optional<core::SceneLoad::Deferral> deferral;
    std::any loadParams;
    std::any enterParams;
    int fixedSteps = 0;
    int updates = 0;
    int renders = 0;
    int uiRenders = 0;

    // Runs with the log entry of every hook right after it is logged, so a test can act in the middle of a change, such as clearing the stack or failing the hook.
    std::function<void(const std::string& entry)> onHook;

  private:
    void record(std::string_view hook) {
        const std::string entry = name + ":" + std::string(hook);
        log.push_back(entry);
        if (onHook) {
            onHook(entry);
        }
    }

    std::string name;
    std::vector<std::string>& log;
    bool transparent;
    core::ProcessMode mode;
};

} // namespace haylen::test
