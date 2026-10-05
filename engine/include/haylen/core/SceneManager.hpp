#pragma once

#include <any>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/FrameClock.hpp"
#include "haylen/core/LoadingView.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneView.hpp"
#include "haylen/core/TransitionEffect.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/EasingCurve.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::core {

// Stack of scenes that changes through one pipeline. A change starts at the next update, loads the next scene of a push or a replace through its load hook and plays its transition on real time, so it runs even while the game is paused. An effect that covers the screen covers the current scenes, changes the stack at full cover, holds the covered frame while the next scene loads and then reveals it. An effect that shows both scenes loads the next scene while the current scenes stay on screen, and then plays with both scenes alive until the leaving scenes exit at its exit point. A change without an effect loads and then switches at once. Changes requested during a change wait for it in request order, and scenes publish their lifecycle and the phases of each transition on the event bus of the engine.
class SceneManager final {
  public:
    struct Transition {
        float duration = 0.0F;
        math::EasingCurve ease;

        // Draws the transition. A transition with a duration needs one.
        std::shared_ptr<TransitionEffect> effect;

        // Holds back input events of the scenes and the action map until the change ends, including the load of the next scene.
        bool blockInput = true;

        // Fades through a color, the classic transition.
        [[nodiscard]] static Transition fade(float seconds, math::Color color = math::Color::black());
    };

    // How a change or a preload ended: done, dropped by a clear or by cancelling the preload, or failed because the next scene could not load.
    enum class Outcome : std::uint8_t {
        Completed,
        Dropped,
        Failed,
    };

    struct Result {
        Outcome outcome = Outcome::Completed;
        std::optional<lua::Error> error;
    };

    using Completion = std::function<void(const Result& result)>;
    using ErrorHandler = std::function<void(const lua::Error& error)>;

    // The options of `push` and `replace`.
    struct Options {
        Transition transition;

        // Reaches the load and enter hooks of the next scene.
        std::any params;

        // Shows while the next scene loads, once the load took longer than `loadingDelay`, and stays for at least `minimumLoadingTime` once it appears. Without a view the covered frame of the transition shows, or the current scenes.
        std::shared_ptr<LoadingView> loading;
        float loadingDelay = 0.0F;
        float minimumLoadingTime = 0.0F;

        // A view over the covered frame of an effect that covers the screen then fades out into it over this many seconds, where zero removes it at once.
        float loadingFadeOut = 0.25F;

        // With an effect that covers the screen, the replaced scene exits and unloads at full cover before the next scene loads, for a lower peak of memory. Turned off, the next scene loads while the transition covers the screen and the replaced scene stays loaded until the next scene loaded.
        bool unloadBeforeLoad = true;

        Completion completion;

        // Receives the error of a failed load instead of the error screen or the log, and may route the app to another scene.
        ErrorHandler onError;
    };

    // The top scenes before and after a change, which the phase events of its transition carry. After a failed load, the scene after is the one the change reveals again.
    struct Transfer {
        const Scene* from = nullptr;
        const Scene* to = nullptr;
    };

    // What `sceneLoadFailed` carries.
    struct LoadFailure {
        const Scene* scene = nullptr;
        const lua::Error* error = nullptr;
    };

    explicit SceneManager(Engine& owner);
    ~SceneManager();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // Brings a scene that is not on the stack. A preloaded scene keeps the `params` of its preload, and a scene that unloaded may come again.
    void push(std::shared_ptr<Scene> scene);
    void push(std::shared_ptr<Scene> scene, Options options);
    void replace(std::shared_ptr<Scene> scene);
    void replace(std::shared_ptr<Scene> scene, Options options);
    void pop();
    void pop(Transition transition, Completion completion = {});

    // Pops scenes until `level` scenes remain, from the top down, and resumes the scene left on top. A stack with `level` scenes or fewer stays as it is.
    void popTo(std::size_t level);
    void popTo(std::size_t level, Transition transition, Completion completion = {});
    void popToRoot();
    void popToRoot(Transition transition, Completion completion = {});

    // Starts loading a scene in the background. A later push or replace of the scene takes it without loading it again, and waits only for the rest of its load.
    void preload(std::shared_ptr<Scene> scene, std::any params = {}, Completion completion = {});

    // Unloads a preloaded scene that no change took, stopping its load when it still runs.
    void cancelPreload(const Scene& scene);

    // Removes every scene at once, from the top down, together with the scenes still leaving, loading or preloaded, and drops pending changes.
    void clear();

    [[nodiscard]] Scene* getTop() const noexcept;

    // Returns the scene at the index, counted from the bottom of the stack.
    [[nodiscard]] Scene& at(std::size_t index) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return stack.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return stack.empty();
    }
    [[nodiscard]] auto begin() const noexcept {
        return stack.begin();
    }
    [[nodiscard]] auto end() const noexcept {
        return stack.end();
    }
    [[nodiscard]] std::optional<std::size_t> find(const Scene& scene) const noexcept;

    [[nodiscard]] bool isTransitioning() const noexcept {
        return pending != nullptr;
    }
    [[nodiscard]] bool isInputBlocked() const noexcept {
        return pending && pending->options.transition.blockInput;
    }
    [[nodiscard]] bool isLoadingViewShown() const noexcept {
        return pending && pending->viewShown;
    }

    // Returns how much of the loading view still shows, which falls from 1 to 0 while it fades out.
    [[nodiscard]] float getLoadingViewOpacity() const noexcept;

    // Returns the mode of the scene at the index with `Inherit` resolved against the scenes below it.
    [[nodiscard]] ProcessMode getProcessMode(std::size_t index) const;

    // Tells every scene that the new pause state stops or starts, from the bottom of the stack up. The engine calls it when the game pause changes.
    void notifyPauseChange(bool gamePaused);

    // Works out the views of the frame about to render: the visible scenes straight onto the screen or, during a transition, the images of the scenes before and after the change and the screen the effect draws on, which renders last.
    void prepareViews();

    // The views that `prepareViews` worked out, in the order they render, which plugins read while the frame renders.
    [[nodiscard]] const std::vector<SceneView>& getViews() const noexcept {
        return views;
    }

    void event(const platform::Event& event);
    void fixedUpdate(const FrameClock& clock);
    void update(const FrameClock& clock);
    void render(const SceneView& view);
    void renderUi(const SceneView& view);

    // Draws the effect of the transition for a view that has one. While the loading view fades out, the current view renders the view over the covered frame into an image of its own, and the effect blends that image over the covered frame of the screen.
    void renderTransition(graphics2d::Renderer& renderer, const SceneView& view);

  private:
    enum class Operation : std::uint8_t {
        Push,
        Replace,
        Pop,
        PopTo,
    };

    enum class Phase : std::uint8_t {
        Start,
        Load,
        Cover,
        Hold,
        Reveal,
    };

    struct Change {
        Operation operation = Operation::Push;
        std::shared_ptr<Scene> scene;
        std::size_t level = 0;
        Options options;
        Phase phase = Phase::Start;
        std::shared_ptr<Scene> from;
        std::optional<lua::Error> failure;
        float elapsed = 0.0F;
        float waited = 0.0F;
        float shown = 0.0F;
        float faded = 0.0F;
        bool covering = false;
        bool viewShown = false;
        bool fading = false;
        bool exited = true;

        // The change had to wait for its load in an earlier update, so the time of the update that ends the wait went to waiting too.
        bool blocked = false;
    };

    // Returns the scenes that render, from the lowest opaque scene up to the top.
    [[nodiscard]] static std::vector<std::shared_ptr<Scene>> getVisible(std::vector<std::shared_ptr<Scene>> scenes);

    // Returns whether the scene has to load before it enters, which a scene that never loaded or that unloaded has.
    [[nodiscard]] static bool needsLoad(const Scene* scene) noexcept;
    [[nodiscard]] static SceneLoad::Progress getLoadingProgress(const Change& change);

    void request(Change change);
    void advance(float deltaSeconds);
    void start(const std::shared_ptr<Change>& change);
    void coverScreen(const std::shared_ptr<Change>& change);
    void revealScene(const std::shared_ptr<Change>& change);
    void restoreScenes(const std::shared_ptr<Change>& change);
    void switchScenes(const std::shared_ptr<Change>& change);
    void finish(const std::shared_ptr<Change>& change);

    // Ends the pending change and makes the next queued change pending.
    void takeNext() noexcept;
    void reportFailure(const Change& change, bool currentKept);

    // Moves time on for the load of the change and its loading view, and returns whether the change may go on.
    [[nodiscard]] bool waitForLoad(const std::shared_ptr<Change>& change, float deltaSeconds);
    void showLoadingView(const std::shared_ptr<Change>& change);
    void hideLoadingView(Change& change);

    void startLoad(const std::shared_ptr<Scene>& scene, std::any params);

    // Turns a finished or failed load into the state and the events of its scene, and settles the preloads that wait for it.
    void settle(const std::shared_ptr<Scene>& scene);
    void settleLoads();
    void notifyWatchers(const Scene& scene, const Result& result);

    // Takes scenes off the stack: they exit and unload at once, or wait in the leaving list for the exit point of an effect that shows both scenes.
    void release(const std::shared_ptr<Scene>& scene, bool exitNow);
    void exitLeaving();
    void enterScene(const std::shared_ptr<Scene>& scene);
    void exitScene(const std::shared_ptr<Scene>& scene);
    void unloadScene(const std::shared_ptr<Scene>& scene);
    void retire(const std::shared_ptr<Scene>& scene);
    void coverScene(const std::shared_ptr<Scene>& scene);
    void uncoverScene(const std::shared_ptr<Scene>& scene);
    void publish(std::string_view name, const Scene& scene);
    void publishPhase(std::string_view name, const Change& change);
    void prepareImages();
    void prepareLoadingImage();
    void endTransition() noexcept;
    [[nodiscard]] std::size_t getKeptLevel(const Change& change) const noexcept;
    [[nodiscard]] float getProgress(const Change& change) const;
    [[nodiscard]] bool isFinished(const Change& change) const;
    [[nodiscard]] bool isAlive(const Scene& scene) const noexcept;
    [[nodiscard]] bool isEffectShown() const noexcept;
    [[nodiscard]] std::shared_ptr<Scene> getTopShared() const noexcept;
    [[nodiscard]] bool canProcessTop(const FrameClock& clock) const;

    Engine& engine;
    std::vector<std::shared_ptr<Scene>> stack;
    std::vector<std::shared_ptr<Scene>> leaving;
    std::vector<std::shared_ptr<Scene>> outgoing;
    std::vector<std::shared_ptr<Scene>> preloaded;
    std::vector<std::pair<std::shared_ptr<Scene>, Completion>> watchers;
    graphics::RenderTarget outgoingImage;
    graphics::RenderTarget incomingImage;
    graphics::RenderTarget loadingImage;
    std::vector<SceneView> views;
    std::shared_ptr<Change> pending;
    std::deque<std::shared_ptr<Change>> queue;
};

} // namespace haylen::core
