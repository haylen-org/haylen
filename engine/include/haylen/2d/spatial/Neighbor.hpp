#pragma once

#include <cstdint>

namespace haylen::spatial2d {

// An entry of a spatial structure found near a point, with the distance from the point to its bounds, which is zero inside them.
struct Neighbor {
    std::uint64_t id = 0;
    float distance = 0.0F;

    [[nodiscard]] bool operator<(const Neighbor& other) const noexcept {
        return distance != other.distance ? distance < other.distance : id < other.id;
    }
};

} // namespace haylen::spatial2d
