#pragma once

#include <cstdint>

namespace haylen::physics2d {

// Shapes collide when each one's category is in the other's mask. Shapes that share a positive group always collide and a negative group never does.
struct CollisionFilter {
    std::uint64_t category = 1;
    std::uint64_t mask = ~std::uint64_t{0};
    int group = 0;
};

} // namespace haylen::physics2d
