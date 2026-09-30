#pragma once

#include <cstdint>

namespace haylen::platform {

// Whether the system shows light or dark colors, which the user may change while the app runs.
enum class Theme : std::uint8_t {
    Light,
    Dark,
};

} // namespace haylen::platform
