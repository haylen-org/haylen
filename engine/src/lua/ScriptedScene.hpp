#pragma once

#include <any>
#include <functional>
#include <memory>

#include "haylen/core/ProcessMode.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/lua/Reference.hpp"

namespace haylen::lua {

// Scene implemented by a Lua table whose optional methods mirror the `Scene` hooks. A table is one scene for as long as the engine holds it, however often it is pushed, preloaded or looked up, and it can come back as a new scene once it unloaded. Its `processMode` and `transparent` fields are read every frame, its load hook runs as a task the scene owns, and everything the table owns ends when the scene unloads.
class ScriptedScene final : public core::Scene {
  public:
    ScriptedScene(lua_State* L, int index);

    // Returns the scene of the table at index, creating it when the engine holds none.
    [[nodiscard]] static std::shared_ptr<ScriptedScene> get(lua_State* L, int index);

    // Returns the scene of the table at index while the engine holds it, or null.
    [[nodiscard]] static std::shared_ptr<ScriptedScene> find(lua_State* L, int index);

    // Returns whether the table at index was ever a scene.
    [[nodiscard]] static bool wasScene(lua_State* L, int index);

    // Pushes the Lua value that `params` carry, or `nil` when they are empty.
    static void pushParams(lua_State* L, const std::any& params);

    void load(core::Engine& engine, core::SceneLoad& context) override;
    void enter(core::Engine& engine, const std::any& params) override;
    void enterTransitionFinished(core::Engine& engine) override;
    void exitTransitionStarted(core::Engine& engine) override;
    void exit(core::Engine& engine) override;
    void unload(core::Engine& engine) override;
    void pause(core::Engine& engine) override;
    void resume(core::Engine& engine) override;
    void paused(core::Engine& engine) override;
    void unpaused(core::Engine& engine) override;
    void event(core::Engine& engine, const platform::Event& event) override;
    void fixedUpdate(core::Engine& engine, float stepSeconds) override;
    void update(core::Engine& engine, float deltaSeconds) override;
    void render(core::Engine& engine) override;
    void renderUi(core::Engine& engine) override;
    [[nodiscard]] bool isTransparent() const override;
    [[nodiscard]] core::ProcessMode getProcessMode() const override;

    void pushTable(lua_State* L) const;

    // Returns a function that resolves the mode of the owner at index on every call, so a timer or tween follows its owner when the mode changes. It returns `Inherit` once the owner is gone, and it must be destroyed while the Lua state is open.
    [[nodiscard]] static std::function<core::ProcessMode()> followOwnerMode(lua_State* L, int owner);

    // Reads a `processMode` field of the table at index, or `Inherit` when it has none.
    [[nodiscard]] static core::ProcessMode readProcessMode(lua_State* L, int index);

  private:
    // Maps scene tables to their scenes with weak keys, so the map holds neither.
    static constexpr const char* kScenes = "haylen.scenes";
    static constexpr const char* kHandleType = "haylen.SceneHandle";

    static void pushScenes(lua_State* L);
    static int collectHandle(lua_State* L);

    // Returns the mode that something owned by the value at index inherits: the resolved mode of the scene on the stack whose table it is, the `processMode` field of any other table, such as an autoload, or `Inherit`.
    [[nodiscard]] static core::ProcessMode resolveOwnerMode(lua_State* L, int owner);

    // Calls the method named `name`, when the table has one, with the table as `self` and the arguments that `pushArguments` pushes. The lookup is protected like the call, because a metatable may raise errors for fields it lacks.
    void call(const char* name, int arguments = 0, const std::function<void(lua_State*)>& pushArguments = {}) const;

    Reference table;
};

} // namespace haylen::lua
