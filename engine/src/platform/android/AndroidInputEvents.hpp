#pragma once

#include <game-activity/GameActivity.h>

#include <cstdint>
#include <optional>

#include "haylen/input/Key.hpp"
#include "haylen/input/KeyModifiers.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::platform {

// The input events of GameActivity, which reach the app as engine events. Controllers and the scrolling of mice and touchpads go to their own readers, keys of hardware keyboards become key events together with the characters they type, and touches and the buttons of mice become touches in framebuffer pixels.
class AndroidInputEvents final {
  public:
    // Answers on the UI thread whether the app wants a new press of a key, which it does for the keys it knows, the keys that type a character and the buttons of controllers. Every other key, such as back and the volume keys, goes on to the views and the system.
    [[nodiscard]] static bool wantsKey(const GameActivityKeyEvent& event);

    // Hand a key or a motion event to the app. They run on the frame thread.
    static void dispatchKey(const GameActivityKeyEvent& event);
    static void dispatchMotion(const GameActivityMotionEvent& event);

  private:
    static void dispatchTouches(const GameActivityMotionEvent& event);
    [[nodiscard]] static bool typesCharacter(const GameActivityKeyEvent& event) noexcept;
    [[nodiscard]] static input::Key toKey(std::int32_t code) noexcept;
    [[nodiscard]] static input::KeyModifiers toModifiers(std::int32_t metaState) noexcept;
    [[nodiscard]] static std::optional<Event::Type> toTouchType(std::int32_t action) noexcept;
};

} // namespace haylen::platform
