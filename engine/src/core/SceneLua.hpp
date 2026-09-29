#pragma once

#include <array>
#include <string_view>

#include "haylen/core/SceneManager.hpp"

struct lua_State;

namespace haylen::core {

// Installs haylen.scene: the scene stack with transitions, loading views, preloads and completion promises, the load context that load hooks receive, the listen and spawn helpers that tie listeners and tasks to an owner, and the Scene base class for scenes written with haylen.class.
class SceneLua final {
  public:
    static void install(lua_State* L);

    // Pushes the context that the load hook of a Lua scene receives. It is released once the scene entered or unloaded.
    static void pushLoad(lua_State* L, SceneLoad& load);

  private:
    static constexpr std::array<std::string_view, 7> kTransitionFields{"duration", "effect", "direction", "color", "ease", "blockInput", "onComplete"};
    static constexpr std::array<std::string_view, 14> kChangeFields{"duration", "effect", "direction", "color", "ease", "blockInput", "onComplete", "onError", "params", "loading", "loadingDelay", "minimumLoadingTime", "loadingFadeOut", "unloadBeforeLoad"};

    [[nodiscard]] static SceneManager::Transition readTransition(lua_State* L, int index);
    [[nodiscard]] static SceneManager::Options readOptions(lua_State* L, int index);

    // Returns the completion of a change or a preload, which calls the onComplete of the options at index, when there are some, and settles the promise it pushes onto the stack.
    [[nodiscard]] static SceneManager::Completion pushCompletion(lua_State* L, int options);
    static void pushScene(lua_State* L, const Scene& scene);
    static void pushTransfer(lua_State* L, const SceneManager::Transfer& transfer);
    static void pushLoadFailure(lua_State* L, const SceneManager::LoadFailure& failure);

    static int push(lua_State* L);
    static int replace(lua_State* L);
    static int pop(lua_State* L);
    static int popTo(lua_State* L);
    static int popToRoot(lua_State* L);
    static int preload(lua_State* L);
    static int cancelPreload(lua_State* L);
    static int clear(lua_State* L);
    static int size(lua_State* L);
    static int top(lua_State* L);
    static int at(lua_State* L);
    static int list(lua_State* L);
    static int transitioning(lua_State* L);
    static int loadingViewOpacity(lua_State* L);
    static int state(lua_State* L);
    static int progress(lua_State* L);
    static int listen(lua_State* L);
    static int spawn(lua_State* L);
    static int loadProgress(lua_State* L);
    static int loadPreload(lua_State* L);
    static int loadParams(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::core
