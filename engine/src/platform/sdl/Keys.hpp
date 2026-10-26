#pragma once

#include <SDL3/SDL_scancode.h>

#include "haylen/input/Key.hpp"

namespace haylen::platform::sdl {

// The keys of the engine for the scancodes of SDL, which name the place of a key on the keyboard whatever its layout, as the keys of the engine do.
class Keys final {
  public:
    [[nodiscard]] static input::Key fromScancode(SDL_Scancode scancode) noexcept;
};

} // namespace haylen::platform::sdl
