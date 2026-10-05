#pragma once

#include <memory>
#include <vector>

#include "haylen/graphics/RenderTarget.hpp"

namespace haylen::core {

class Scene;

// What one image of a frame shows: the scenes to render from the bottom up and the render target they go into, which stays empty when they draw straight onto the screen. The current view holds the scenes the app runs, and it also takes the loading view, the drawing of the engine plugins and everything of the interface that belongs to no scene. The effect view draws the transition before its scenes.
struct SceneView {
    std::vector<std::shared_ptr<Scene>> scenes;
    graphics::RenderTarget target;
    bool current = false;
    bool effect = false;
};

} // namespace haylen::core
