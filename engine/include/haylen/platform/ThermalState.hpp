#pragma once

#include <cstdint>

namespace haylen::platform {

// How hot the device runs as the system judges it, from `Nominal` to `Critical`, where the system slows the device down to cool it, so the app draws less to keep its frames smooth. `Unknown` stands where the platform does not tell.
enum class ThermalState : std::uint8_t {
    Unknown,
    Nominal,
    Fair,
    Serious,
    Critical,
};

} // namespace haylen::platform
