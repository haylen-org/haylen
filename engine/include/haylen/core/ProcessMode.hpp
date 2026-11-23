#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/core/EnumNames.hpp"

namespace haylen::core {

// How something behaves while the engine is paused. Scenes, autoloads, timers, tweens and sounds each have one. The mode `Inherit` takes the mode of the parent, such as the scene below on the stack or the scene that owns a tween, and resolves to `Pausable` at the root.
enum class ProcessMode : std::uint8_t {
    Inherit,
    Pausable,
    WhenPaused,
    Always,
    Disabled,
};

template <> struct EnumNames<ProcessMode> {
    static constexpr std::array<std::pair<std::string_view, ProcessMode>, 5> kNames{{{"inherit", ProcessMode::Inherit}, {"pausable", ProcessMode::Pausable}, {"whenPaused", ProcessMode::WhenPaused}, {"always", ProcessMode::Always}, {"disabled", ProcessMode::Disabled}}};

    static std::optional<ProcessMode> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, ProcessMode>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(ProcessMode value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, ProcessMode>::second)->first;
    }
};

} // namespace haylen::core
