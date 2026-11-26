#pragma once

#include <DetourNavMeshQuery.h>

#include <cstdint>

#include "haylen/3d/navigation/QueryFilter.hpp"

namespace haylen::navigation3d {

// The filter of Detour that a `QueryFilter` turns into: it passes the polygons of the areas the filter does not exclude and weighs each step by the cost of the area of its polygon.
class AreaFilter final : public dtQueryFilter {
  public:
    explicit AreaFilter(const QueryFilter& filter) noexcept;

    bool passFilter(dtPolyRef ref, const dtMeshTile* tile, const dtPoly* poly) const override;

  private:
    std::uint64_t excludedAreas = 0;
};

} // namespace haylen::navigation3d
