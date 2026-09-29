#pragma once

#include <cstdint>
#include <optional>

#include "haylen/input/Key.hpp"

namespace haylen::platform {

// The keys of TV remotes that reach Android as raw input events, which sokol_app does not translate: the directional pad, select, back and play and pause. Back becomes escape, which the UI reads as ui_cancel, and it stays with Android, which leaves the app, unless the app captures it.
class AndroidKeys final {
  public:
    // Takes a raw input event before sokol_app sees it and returns whether the app took it. It runs on the frame thread.
    static bool handleEvent(const void* source);

  private:
    [[nodiscard]] static std::optional<input::Key> toKey(std::int32_t code) noexcept;

    // Whether the app took the last press of back, or the text input took it to dismiss a field, so its release goes to the same place.
    static bool backTaken;
    static bool backDismissed;
};

} // namespace haylen::platform
