#pragma once

#include <optional>
#include <vector>

#include "haylen/platform/Event.hpp"

namespace haylen::platform {

// What the platform reported that lasts until it reports otherwise: whether the device reaches the network, whether the app is in the background, whether its window has the focus and whether an interruption of the system holds it, together with the pause of a page. Every app that the runtime starts hears it first, so an app that restarts in the background, without the focus or during a call starts that way.
class ReportedState final {
  public:
    // Records an event and returns whether the running app hears it. A suspension or a resumption reaches the app only when it changes whether the app is suspended, which the background and the pause decide together.
    [[nodiscard]] bool record(const Event& event) noexcept;

    // Pauses or resumes the app, and returns the event the running app hears, if any.
    [[nodiscard]] std::optional<Event> setPaused(bool value) noexcept;
    [[nodiscard]] bool isPaused() const noexcept {
        return paused;
    }

    // The events that bring a new app to this state, in order.
    [[nodiscard]] std::vector<Event> getReplay() const;

  private:
    [[nodiscard]] bool isSuspended() const noexcept {
        return background || paused;
    }

    std::optional<bool> online;
    bool background = false;
    bool paused = false;
    bool focused = true;
    bool interrupted = false;
};

} // namespace haylen::platform
