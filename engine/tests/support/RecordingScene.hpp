#pragma once

#include <any>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneLoad.hpp"
#include "haylen/lua/Error.hpp"

namespace haylen::test {

// Scene that writes every hook it receives to a shared log as name:hook. Its load finishes when the hook returns, waits for a deferral that the test completes, or fails, and it can preload asset groups.
class RecordingScene final : public core::Scene {
  public:
    enum class Load {
        Instant,
        Deferred,
        Failing,
    };

    RecordingScene(std::string label, std::vector<std::string>& entries, bool see = false, core::ProcessMode processMode = core::ProcessMode::Inherit) : name(std::move(label)), log(entries), transparent(see), mode(processMode) {}

    void load(core::Engine&, core::SceneLoad& context) override {
        log.push_back(name + ":load");
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
        log.push_back(name + ":enter");
        enterParams = params;
    }
    void enterTransitionFinished(core::Engine&) override {
        log.push_back(name + ":enterTransitionFinished");
    }
    void exitTransitionStarted(core::Engine&) override {
        log.push_back(name + ":exitTransitionStarted");
    }
    void exit(core::Engine&) override {
        log.push_back(name + ":exit");
    }
    void unload(core::Engine&) override {
        log.push_back(name + ":unload");
    }
    void pause(core::Engine&) override {
        log.push_back(name + ":pause");
    }
    void resume(core::Engine&) override {
        log.push_back(name + ":resume");
    }
    void paused(core::Engine&) override {
        log.push_back(name + ":paused");
    }
    void unpaused(core::Engine&) override {
        log.push_back(name + ":unpaused");
    }
    void event(core::Engine&, const platform::Event&) override {
        log.push_back(name + ":event");
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
    [[nodiscard]] const std::string& getName() const noexcept {
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

  private:
    std::string name;
    std::vector<std::string>& log;
    bool transparent;
    core::ProcessMode mode;
};

} // namespace haylen::test
