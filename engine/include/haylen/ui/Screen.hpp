#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/ui/Placement.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::ui {

class Gui;
class Navigation;
class NavigationStack;
class Presentation;

// One page of an app, such as a feed, a profile or a step of a new post, which a navigation stack or a presentation shows. A screen builds its GUI the first time it shows, and the GUI then lives as long as the screen, so it keeps its scroll, its focus and its fields while other screens cover it or another tab shows. The hooks tell the screen when it starts and ends to show, run every frame while it shows, ask it before the system back takes it away and tell it when it leaves for good, after which its GUI unmounts.
class Screen {
  public:
    enum class State : std::uint8_t {
        Created,
        Appearing,
        Shown,
        Disappearing,
        Hidden,
        Removed,
    };

    virtual ~Screen() = default;

    // Resolves the state names `created`, `appearing`, `shown`, `disappearing`, `hidden` and `removed`.
    [[nodiscard]] static std::optional<State> stateFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view stateName(State value) noexcept;

    // Builds the GUI of the screen, not mounted yet, with the placement that the navigation mounts it with, the first time the screen shows.
    [[nodiscard]] virtual std::shared_ptr<Gui> build(core::Engine& engine, Placement placement) = 0;

    // The screen starts to show, as it comes on top of its stack, its presentation opens or its tab shows again, and it appeared once its transition ended and input reaches it.
    virtual void appear(core::Engine& engine);
    virtual void appeared(core::Engine& engine);

    // The screen starts to leave the screen, and it disappeared once it no longer shows.
    virtual void disappear(core::Engine& engine);
    virtual void disappeared(core::Engine& engine);

    // Runs every frame in which the screen draws, on real time.
    virtual void update(core::Engine& engine, float deltaSeconds);

    // Draws what the screen shows besides its GUI, every frame in which the screen draws and before the UI draws, such as the images that its GUI shows.
    virtual void render(core::Engine& engine);

    // Hears that the system back is about to take the screen away, and returns true to keep it, such as to ask first whether to drop a draft.
    [[nodiscard]] virtual bool back(core::Engine& engine);

    // The screen left its stack or its presentation for good, and its GUI unmounts right after.
    virtual void remove(core::Engine& engine);

    [[nodiscard]] State getState() const noexcept {
        return state;
    }

    // The GUI of the screen, which stays empty until the screen first shows.
    [[nodiscard]] const std::shared_ptr<Gui>& getGui() const noexcept {
        return gui;
    }

    // The stack that holds the screen, or nothing for a presented screen or one that no stack holds.
    [[nodiscard]] std::shared_ptr<NavigationStack> getStack() const noexcept {
        return stack.lock();
    }

    // The presentation that shows the screen, or nothing for a screen of a stack.
    [[nodiscard]] std::shared_ptr<Presentation> getPresentation() const noexcept {
        return presentation.lock();
    }

  protected:
    // The screen joined a stack or a presentation, or moved to another stack when a split collapses or expands.
    virtual void placed(core::Engine& engine);

  private:
    friend class Navigation;
    friend class NavigationStack;
    friend class Presentation;

    static const std::array<std::pair<std::string_view, State>, 6> kStateNames;

    State state = State::Created;
    std::shared_ptr<Gui> gui;
    std::weak_ptr<NavigationStack> stack;
    std::weak_ptr<Presentation> presentation;
};

} // namespace haylen::ui
