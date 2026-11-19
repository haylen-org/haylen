#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/platform/Display.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::core {
class EventBus;
}

namespace haylen::platform {

// The displays of the device as the app sees them: the list that the engine reads when it starts and again whenever the platform reports a change, which publishes `displayConnected`, `displayDisconnected` and `displayChanged` for each display that the change touched, and the display that shows the window, whose change publishes `windowDisplayChanged`. A browser lists every screen once the person lets the page see them through `requestEveryScreen`. The engine owns it, and every method and callback runs on the frame thread.
class Displays final {
  public:
    Displays(Window& window, core::EventBus& events);

    Displays(const Displays&) = delete;
    Displays& operator=(const Displays&) = delete;

    [[nodiscard]] std::span<const Display> getAll() const noexcept;

    // Returns the display with the id, or null when no such display is connected.
    [[nodiscard]] const Display* find(std::string_view id) const noexcept;

    // Returns the display that shows the window. A window on a display that the engine has not heard of yet, between a move and the report of the displays that follows it, counts as on the primary display.
    [[nodiscard]] const Display& getWindowDisplay() const noexcept;

    // Whether the device has displays that the list leaves out until the person lets the app see every screen, as a browser tells before `requestEveryScreen`.
    [[nodiscard]] bool hasUnlisted() const noexcept {
        return unlisted;
    }

    // Asks the platform to list every display, as `Window::requestEveryScreen` describes, and calls back as the window does, which `refresh` follows to read the displays that a granted request added.
    void requestEveryScreen(std::function<void(SystemAnswer answer)> callback);

    // Reads the displays again and publishes what changed, the displays that went away first.
    void refresh();

    // Reads the display of the window again and publishes a change.
    void refreshWindowDisplay();

  private:
    [[nodiscard]] const Display& getPrimary() const noexcept;

    Window& window;
    core::EventBus& events;
    std::vector<Display> displays;
    std::string windowDisplay;
    bool unlisted = false;
};

} // namespace haylen::platform
