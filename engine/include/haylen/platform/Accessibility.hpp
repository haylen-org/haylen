#pragma once

#include "haylen/platform/AccessibilitySettings.hpp"

namespace haylen::core {
class EventBus;
}

namespace haylen::platform {

class Host;

// The accessibility of the running app: the settings of the system, which the engine publishes as `accessibilitySettingsChanged` once per change. Every method runs on the frame thread.
class Accessibility final {
  public:
    Accessibility(Host& host, core::EventBus& events);

    Accessibility(const Accessibility&) = delete;
    Accessibility& operator=(const Accessibility&) = delete;

    [[nodiscard]] const AccessibilitySettings& getSettings() const noexcept {
        return settings;
    }

    // Publishes the change of the settings that the platform reported since the last pump. The engine calls it at the start of every frame.
    void pump();

  private:
    Host& host;
    core::EventBus& events;
    AccessibilitySettings settings;
};

} // namespace haylen::platform
