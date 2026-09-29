#pragma once

namespace haylen::input {

// The modifier keys held while an input event happened.
struct KeyModifiers {
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool super = false;

    [[nodiscard]] constexpr bool operator==(const KeyModifiers&) const noexcept = default;
};

} // namespace haylen::input
