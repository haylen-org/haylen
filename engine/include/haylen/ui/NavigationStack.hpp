#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "haylen/core/Signal.hpp"
#include "haylen/ui/NavigationTransition.hpp"
#include "haylen/ui/Region.hpp"

namespace haylen::ui {

class Navigation;
class Screen;

// Screens on top of each other in the area of a node of a GUI, such as the middle of an app between its bars, where only the top screen shows. Every change of the stack is a new list of screens: a push adds a screen on top, a pop takes the top away, and a deep link sets the whole list. The screen that leaves and the screen that comes play the transition of the change inside the area while the GUI around them stays, and they take no input until it ends. A change while another one plays finishes the other at once. A hidden stack, such as the stack of a tab that does not show, keeps its screens with their state, and a closed stack removes them all.
class NavigationStack final : public std::enable_shared_from_this<NavigationStack> {
  public:
    // Navigation creates stacks, which register with it.
    NavigationStack(Navigation& owner, Region region);
    ~NavigationStack();

    NavigationStack(const NavigationStack&) = delete;
    NavigationStack& operator=(const NavigationStack&) = delete;

    // Throws `std::invalid_argument` for a screen that another stack or a presentation holds, or one that the list holds twice, and `std::logic_error` once the stack closed.
    void push(std::shared_ptr<Screen> screen, const NavigationTransition& transition = {});
    void pop(const NavigationTransition& transition = {});

    // Pops the screens above a screen of the stack, and throws `std::invalid_argument` for a screen that the stack does not hold.
    void popTo(const Screen& screen, const NavigationTransition& transition = {});
    void popToRoot(const NavigationTransition& transition = {});

    // Puts a screen in the place of the top screen.
    void replace(std::shared_ptr<Screen> screen, const NavigationTransition& transition = {});

    // Makes the list of screens, from the root to the top, as a deep link does. The change goes back when the new top screen was below the old one, and forward otherwise.
    void setScreens(std::vector<std::shared_ptr<Screen>> next, const NavigationTransition& transition = {});

    // Removes every screen at once, and the stack takes no changes anymore.
    void close();

    [[nodiscard]] const std::vector<std::shared_ptr<Screen>>& getScreens() const noexcept {
        return screens;
    }
    [[nodiscard]] std::shared_ptr<Screen> getTop() const noexcept {
        return screens.empty() ? nullptr : screens.back();
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return screens.size();
    }
    [[nodiscard]] bool empty() const noexcept {
        return screens.empty();
    }

    // Whether the system back pops the stack, which it does while a screen lies under the top one.
    [[nodiscard]] bool canGoBack() const noexcept {
        return screens.size() > 1;
    }
    [[nodiscard]] bool isTransitioning() const noexcept {
        return change.has_value();
    }
    [[nodiscard]] bool isClosed() const noexcept {
        return closed;
    }

    // A hidden stack draws nothing, and its top screen disappears until it shows again.
    void setShown(bool value);
    [[nodiscard]] bool isShown() const noexcept {
        return shown;
    }

    [[nodiscard]] const Region& getRegion() const noexcept {
        return region;
    }

    // Tells that the list of screens changed, once the change starts.
    core::Signal<NavigationStack&> changed;

  private:
    friend class Navigation;
    friend class NavigationSplit;

    struct Change {
        std::shared_ptr<Screen> outgoing;
        std::shared_ptr<Screen> incoming;
        std::vector<std::shared_ptr<Screen>> removed;
        NavigationTransition transition;
        bool backward = false;
        float elapsed = 0.0F;
    };

    void checkOpen() const;
    void checkJoining(const std::vector<std::shared_ptr<Screen>>& next) const;

    // Starts a change to the list of screens, which plays its transition from the next frame or ends at once.
    void apply(std::vector<std::shared_ptr<Screen>> next, const NavigationTransition& transition);
    void finish();
    void release(const std::shared_ptr<Screen>& screen);

    // Whether the top screen shows: the stack shows, and so does the screen whose GUI holds its area, if any.
    [[nodiscard]] bool isActive() const;

    // Moves the transition on and finishes it once it played.
    void advance(float deltaSeconds);

    // Takes the screens from the index up out of the stack without hooks or transition, and puts screens on top the same way, which a split does as it collapses and expands.
    [[nodiscard]] std::vector<std::shared_ptr<Screen>> detach(std::size_t index);
    void attach(std::vector<std::shared_ptr<Screen>> moved);

    [[nodiscard]] float getProgress() const;

    Navigation& navigation;
    Region region;
    std::vector<std::shared_ptr<Screen>> screens;
    std::optional<Change> change;
    std::uint64_t generation = 0;
    bool shown = true;
    bool closed = false;
};

} // namespace haylen::ui
