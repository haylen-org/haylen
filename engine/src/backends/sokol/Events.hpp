#pragma once

#include <cstdint>
#include <optional>

#include "haylen/platform/Event.hpp"
#include "sokol_app.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace haylen::sokol {

// Converts `sokol_app` events into engine events.
class Events final {
  public:
    // Returns nothing for events the engine does not use.
    [[nodiscard]] static std::optional<platform::Event> translate(const sapp_event& source) noexcept;

    // Whether the event is a press or release of the back button of the platform, the Menu button of the Apple TV remote.
    [[nodiscard]] static bool isPlatformBack(const sapp_event& source) noexcept;

  private:
    // The Apple TV remote reports its touch surface as touches, which the gamepad of the remote stands for instead, and its Menu button as the menu key, which the engine reads as escape.
#if defined(__APPLE__) && TARGET_OS_TV
    static constexpr bool kRemote = true;
#else
    static constexpr bool kRemote = false;
#endif

    [[nodiscard]] static input::KeyModifiers toModifiers(std::uint32_t bits) noexcept;
    [[nodiscard]] static std::optional<input::MouseButton> toMouseButton(sapp_mousebutton button) noexcept;
    [[nodiscard]] static platform::Event::ScrollPhase toScrollPhase(sapp_scroll_phase phase) noexcept;
    [[nodiscard]] static platform::Event toTouchEvent(platform::Event::Type type, const sapp_event& source) noexcept;
};

} // namespace haylen::sokol
