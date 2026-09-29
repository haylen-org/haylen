#pragma once

#include <cstdint>

namespace haylen::platform {

// Orientations of the screen. An app declares the ones it supports in app.json and can lock them at run time, while the screen itself always reports Landscape or Portrait.
enum class Orientation : std::uint8_t {
    Landscape,
    Portrait,
    Any,
};

} // namespace haylen::platform
