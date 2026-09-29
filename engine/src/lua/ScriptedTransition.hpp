#pragma once

#include "haylen/core/TransitionEffect.hpp"
#include "haylen/lua/Reference.hpp"

namespace haylen::lua {

// Scene transition effect implemented by a Lua table. Its switchProgress field, 0.5 by default, is the eased progress at which it covers the screen, or 0 for an effect that shows both scenes, whose exitProgress field, 1 by default, is where the leaving scenes exit. Its render method receives the eased progress and the outgoing and incoming images, and draws on a canvas it begins itself.
class ScriptedTransition final : public core::TransitionEffect {
  public:
    // Reads the table at index. It needs a render method and a switch progress between 0 and 1, and only an effect that shows both scenes takes an exit progress, between 0 and 1.
    ScriptedTransition(lua_State* L, int index);

    [[nodiscard]] float getSwitchProgress() const noexcept override {
        return switchProgress;
    }
    [[nodiscard]] float getExitProgress() const noexcept override {
        return exitProgress;
    }
    void render(graphics2d::Renderer& renderer, const Frames& frames, float progress) override;

  private:
    Reference table;
    float switchProgress = 0.5F;
    float exitProgress = 0.5F;
};

} // namespace haylen::lua
