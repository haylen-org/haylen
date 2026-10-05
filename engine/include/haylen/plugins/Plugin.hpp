#pragma once

#include <string_view>

struct lua_State;

namespace haylen::platform {
struct Event;
}

namespace haylen::core {
class Engine;
struct SceneView;
} // namespace haylen::core

namespace haylen::plugins {

// A self-contained engine module. Built-in subsystems and extensions from other projects use this same interface, and every hook runs on the frame thread.
class Plugin {
  public:
    virtual ~Plugin() = default;

    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;

    virtual void start(core::Engine& engine);
    virtual void stop(core::Engine& engine);
    virtual void installLua(core::Engine& engine, lua_State* L);
    virtual void event(core::Engine& engine, const platform::Event& event);
    virtual void beginFrame(core::Engine& engine, float deltaSeconds);
    virtual void fixedUpdate(core::Engine& engine, float stepSeconds);
    virtual void update(core::Engine& engine, float deltaSeconds);
    virtual void render(core::Engine& engine);

    // Draws the interface of one view of the frame above its scenes. Every view gets the call in the order the views render, and most plugins draw only in the current view, while the UI draws the GUIs of the scenes of each view into its image.
    virtual void renderUi(core::Engine& engine, const core::SceneView& view);

    // Draws above everything else once per frame, including scene transitions and the error screen, which suits debug displays.
    virtual void renderOverlay(core::Engine& engine);
    virtual void endFrame(core::Engine& engine);

    // Whether the plugin takes the next press of the back button of the platform, such as the UI while a popup is open, so the press never leaves the app.
    [[nodiscard]] virtual bool isCapturingBack() const;
};

} // namespace haylen::plugins
