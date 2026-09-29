#pragma once

#include <cstdint>
#include <string>

namespace haylen::debug {

// The time one profiler scope took within a frame, added up over every call with the same name under the same parent.
struct ProfileSample {
    std::string name;
    double milliseconds = 0.0;
    std::uint32_t calls = 0;
    int depth = 0;
    int parent = -1;
};

} // namespace haylen::debug
