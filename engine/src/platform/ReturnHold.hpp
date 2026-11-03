#pragma once

#include <cstdint>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

struct Event;

// The hold that leaves an app that another app launched, on every kind of device: Escape held on a keyboard, the back button of the platform or of a gamepad held, or three fingers held still on a touch screen. The app receives the same input as always, and only a press held for the whole hold leaves it, so the launched app can never keep the person in it.
class ReturnHold final {
  public:
    static constexpr float kSeconds = 1.5F;

    // A hold shows its progress only after this long, so a short press shows nothing.
    static constexpr float kHintDelay = 0.3F;

    static constexpr int kFingers = 3;

    // How far a finger of the hold moves, in the points of the screen, before the hold counts as a gesture of the app.
    static constexpr float kFingerTravel = 24.0F;

    // Follows the keys and the fingers of an event, whose positions take the scale of pixels to the points of the screen.
    void handleEvent(const Event& event, float pointScale);

    // Counts the seconds that passed while a hold lasts, with whether the back button of the platform or of a gamepad is held, and returns whether the hold just completed.
    [[nodiscard]] bool update(float seconds, bool backHeld);

    // How far the hold is, from 0 to 1, once it lasted longer than the delay of the hint, or 0.
    [[nodiscard]] float getProgress() const noexcept;

  private:
    struct Finger {
        std::uint64_t id = 0;
        math::Vec2 start;
        bool still = true;
    };

    [[nodiscard]] bool isHolding(bool backHeld) const noexcept;

    std::vector<Finger> fingers;
    bool escapeHeld = false;
    float held = 0.0F;
    bool completed = false;
};

} // namespace haylen::platform
