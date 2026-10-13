#pragma once

#include <cstdint>

namespace haylen::input {

// How a gamepad labels its face buttons, by their layout: letters with A at the bottom and B on the right, letters with B at the bottom and A on the right, or shapes. Prompts pick their pictures from it, while bindings keep naming the buttons by their place.
enum class GamepadLabels : std::uint8_t {
    Unknown,
    Letters,
    SwappedLetters,
    Shapes,
};

} // namespace haylen::input
