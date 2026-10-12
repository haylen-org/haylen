#pragma once

#include <cstdint>

namespace haylen::content {

// When the encrypted bytes of a file must be on the device: before the app starts, in the background after it starts, or only when the app asks for the file. It never decides when a file is decoded into memory.
enum class Delivery : std::uint8_t {
    Required = 0,
    Prefetch = 1,
    OnDemand = 2,
};

} // namespace haylen::content
