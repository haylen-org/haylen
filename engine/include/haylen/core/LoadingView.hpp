#pragma once

#include "haylen/core/SceneLoad.hpp"

namespace haylen::core {

class Engine;

// What a scene change shows while its next scene loads: over the covered screen during the hold of an effect that covers the screen, or over the current scenes before a change without an effect or with an effect that shows both scenes. It enters when it appears, receives the progress of the load in every other hook and exits when the load is done, and it draws like the top scene, before the drawing of the engine plugins. It runs on real time, so it animates while the game is paused.
class LoadingView {
  public:
    virtual ~LoadingView() = default;

    virtual void enter(Engine& engine);
    virtual void exit(Engine& engine);
    virtual void update(Engine& engine, float deltaSeconds, const SceneLoad::Progress& progress);
    virtual void render(Engine& engine, const SceneLoad::Progress& progress);
    virtual void renderUi(Engine& engine, const SceneLoad::Progress& progress);
};

} // namespace haylen::core
