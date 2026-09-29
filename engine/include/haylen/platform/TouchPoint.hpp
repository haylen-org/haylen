#pragma once

#include <cstdint>

#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

struct TouchPoint {
    std::uint64_t id = 0;
    math::Vec2 position{};
    bool changed = false;
};

} // namespace haylen::platform
