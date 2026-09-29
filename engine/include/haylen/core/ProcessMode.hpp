#pragma once

#include <cstdint>

namespace haylen::core {

// How something behaves while the engine is paused, like the process modes of Godot. Scenes, autoloads, timers, tweens and sounds each have one. Inherit takes the mode of the parent, such as the scene below on the stack or the scene that owns a tween, and resolves to Pausable at the root.
enum class ProcessMode : std::uint8_t {
    Inherit,
    Pausable,
    WhenPaused,
    Always,
    Disabled,
};

} // namespace haylen::core
