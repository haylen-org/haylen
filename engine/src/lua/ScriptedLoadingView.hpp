#pragma once

#include <functional>

#include "haylen/core/LoadingView.hpp"
#include "haylen/lua/Reference.hpp"

namespace haylen::lua {

// Loading view implemented by a Lua table whose optional methods mirror the scene hooks, so a scene table can serve as one. The hooks after `enter` receive the progress of the load and its message after their own arguments, and everything the table owns ends when the view exits.
class ScriptedLoadingView final : public core::LoadingView {
  public:
    ScriptedLoadingView(lua_State* L, int index);

    void enter(core::Engine& engine) override;
    void exit(core::Engine& engine) override;
    void update(core::Engine& engine, float deltaSeconds, const core::SceneLoad::Progress& progress) override;
    void render(core::Engine& engine, const core::SceneLoad::Progress& progress) override;
    void renderUi(core::Engine& engine, const core::SceneLoad::Progress& progress) override;

  private:
    // Calls the method named `name`, when the table has one, with the table as `self`, the arguments that `pushArguments` pushes and then the progress.
    void call(const char* name, const core::SceneLoad::Progress* progress, int arguments = 0, const std::function<void(lua_State*)>& pushArguments = {}) const;

    Reference table;
};

} // namespace haylen::lua
