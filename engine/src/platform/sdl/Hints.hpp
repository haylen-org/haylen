#pragma once

#include "haylen/core/AppConfig.hpp"

namespace haylen::platform::sdl {

// The metadata of the app and the hints that SDL starts with, which decide how the window, the pointer, the pen and the controllers behave before SDL opens any of them.
class Hints final {
  public:
    static void apply(const core::AppConfig& config);
};

} // namespace haylen::platform::sdl
