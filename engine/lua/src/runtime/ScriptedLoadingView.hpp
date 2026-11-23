#pragma once

#include <functional>

#include "haylen/core/LoadingView.hpp"
#include "haylen/lua/Reference.hpp"

namespace haylen::lua {

// Loading view implemented by a Lua table whose optional methods mirror the scene hooks, so a scene table can serve as one. The hooks after `enter` receive the progress of the load and its message after their own arguments, and everything the table owns ends when the view exits.
class ScriptedLoadingView final : public haylen::core::LoadingView {
  public:
    ScriptedLoadingView(lua_State* L, int index);

    void enter(haylen::core::Engine& engine) override;
    void exit(haylen::core::Engine& engine) override;
    void update(haylen::core::Engine& engine, float deltaSeconds, const haylen::core::SceneLoad::Progress& progress) override;
    void render(haylen::core::Engine& engine, const haylen::core::SceneLoad::Progress& progress) override;
    void renderUi(haylen::core::Engine& engine, const haylen::core::SceneLoad::Progress& progress) override;

  private:
    // Calls the method named `name`, when the table has one, with the table as `self`, the arguments that `pushArguments` pushes and then the progress.
    void call(const char* name, const haylen::core::SceneLoad::Progress* progress, int arguments = 0, const std::function<void(lua_State*)>& pushArguments = {}) const;

    Reference table;
};

} // namespace haylen::lua
