#pragma once

#include <cstdint>

#include "haylen/3d/navigation/Link.hpp"

namespace haylen::navigation3d {

// A link that a navigation mesh holds, with its id, which the polygon of the link carries, and the number of its area.
struct MeshLink {
    std::uint32_t id = 0;
    Link link;
    std::uint8_t area = 1;

    friend bool operator==(const MeshLink&, const MeshLink&) = default;
};

} // namespace haylen::navigation3d
