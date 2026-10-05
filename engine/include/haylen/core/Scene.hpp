#pragma once

#include <any>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/core/Connection.hpp"
#include "haylen/core/ConnectionScope.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/core/SceneLoad.hpp"
#include "haylen/core/Signal.hpp"

namespace haylen::platform {
struct Event;
}

namespace haylen::core {

class Engine;

// A screen of the app, such as a menu or a level. Every scene goes through the same states: it loads, enters the stack, is covered while other scenes sit on top of it, exits and unloads, and a scene that unloaded can load again. Only the top scene receives events and updates, when it is not covered and its process mode lets it run in the current pause state, and scenes below a transparent scene keep rendering. Connections a scene listens with, and any connection added to its scope, end when it unloads.
class Scene {
  public:
    enum class State : std::uint8_t {
        Created,
        Loading,
        Loaded,
        Entering,
        Active,
        Covered,
        Exiting,
        Exited,
        Unloaded,
    };

    virtual ~Scene() = default;

    // Resolves the state names `created`, `loading`, `loaded`, `entering`, `active`, `covered`, `exiting`, `exited` and `unloaded`.
    [[nodiscard]] static std::optional<State> stateFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view stateName(State value) noexcept;

    // The name the debug overlay shows the scene by, `Scene` unless the scene gives one.
    [[nodiscard]] virtual std::string getName() const;

    // Loads what the scene needs before it enters, while the transition covers the screen or before an effect that shows both scenes starts. The load finishes when the hook returns, or later through the deferrals and preloads of the load. An exception fails the load.
    virtual void load(Engine& engine, SceneLoad& context);

    // Enters the stack with the `params` of the change.
    virtual void enter(Engine& engine, const std::any& params);

    // The top scene after a change hears when its transition finished and input reaches it again.
    virtual void enterTransitionFinished(Engine& engine);

    // The top scene before a change hears when the transition starts to take it off the screen.
    virtual void exitTransitionStarted(Engine& engine);
    virtual void exit(Engine& engine);

    // Releases what the scene loaded. Every scene that started loading unloads once, after its exit, when its load failed or when a preload is cancelled, so it must also release a partial load.
    virtual void unload(Engine& engine);

    // Another scene was pushed on top of the scene, or the scenes above it were popped.
    virtual void pause(Engine& engine);
    virtual void resume(Engine& engine);

    // The game pause stops or starts the scene by its process mode. A scene is paused when the change stops it and unpaused when the change lets it run again, so a scene that runs only while the game is paused is unpaused when the game pauses.
    virtual void paused(Engine& engine);
    virtual void unpaused(Engine& engine);

    virtual void event(Engine& engine, const platform::Event& event);
    virtual void fixedUpdate(Engine& engine, float stepSeconds);
    virtual void update(Engine& engine, float deltaSeconds);
    virtual void render(Engine& engine);
    virtual void renderUi(Engine& engine);

    [[nodiscard]] virtual bool isTransparent() const;

    // The mode `Inherit`, the default, takes the mode of the scene below and resolves to `Pausable` at the bottom of the stack.
    [[nodiscard]] virtual ProcessMode getProcessMode() const;

    [[nodiscard]] State getState() const noexcept {
        return state;
    }

    // Returns the progress of the running load, 1 once the scene loaded and 0 before it starts loading or after it unloaded.
    [[nodiscard]] SceneLoad::Progress getLoadProgress() const;

    // Connects the slot until the scene unloads.
    template <typename... Args> Connection listen(Signal<Args...>& signal, typename Signal<Args...>::Slot slot) {
        Connection connection = signal.connect(std::move(slot));
        connections.add(connection);
        return connection;
    }

    [[nodiscard]] ConnectionScope& getConnections() noexcept {
        return connections;
    }

  private:
    friend class SceneManager;

    static const std::array<std::pair<std::string_view, State>, 9> kStateNames;

    State state = State::Created;
    std::shared_ptr<SceneLoad> loading;
    ConnectionScope connections;
};

} // namespace haylen::core
