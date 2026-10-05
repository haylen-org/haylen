#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::input {

// Makes the mouse act as a finger and a finger act as the mouse when the app asks for it, so an app made for touch screens runs with a mouse and one that reads the mouse runs on a touch screen.
class PointerEmulation final {
  public:
    // The touch id of the finger the left mouse button becomes, which no platform gives a real finger and Lua holds exactly.
    static constexpr std::uint64_t kMouseTouchId = (1ULL << 53U) - 1U;

    [[nodiscard]] bool isMouseAsTouch() const noexcept {
        return mouseAsTouch;
    }
    void setMouseAsTouch(bool value) noexcept;
    [[nodiscard]] bool isTouchAsMouse() const noexcept {
        return touchAsMouse;
    }
    void setTouchAsMouse(bool value) noexcept;

    // Returns the events that take the place of a platform event, in order: the event itself when nothing changes it, the finger the left mouse button becomes, or the mouse events a finger drives followed by the touch event.
    [[nodiscard]] std::span<const platform::Event> convert(const platform::Event& event);

  private:
    [[nodiscard]] std::span<const platform::Event> convertMouse(const platform::Event& event);
    [[nodiscard]] std::span<const platform::Event> convertTouch(const platform::Event& event);
    void addMouse(platform::Event::Type type, math::Vec2 position);

    std::array<platform::Event, 3> converted{};
    std::size_t count = 0;
    bool mouseAsTouch = false;
    bool touchAsMouse = false;
    bool mouseFingerDown = false;

    // The finger that drives the mouse, which is the first finger down while no other drives it.
    std::optional<std::uint64_t> mouseFinger;
};

} // namespace haylen::input
