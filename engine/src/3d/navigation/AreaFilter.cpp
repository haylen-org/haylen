#include "3d/navigation/AreaFilter.hpp"

#include <DetourNavMesh.h>

namespace haylen::navigation3d {

AreaFilter::AreaFilter(const QueryFilter& filter) noexcept {
    for (std::uint8_t area = 0; area < QueryFilter::kAreaCount; ++area) {
        setAreaCost(area, filter.getCost(area));
        if (filter.isExcluded(area)) {
            excludedAreas |= std::uint64_t{1} << area;
        }
    }
}

bool AreaFilter::passFilter(dtPolyRef, const dtMeshTile*, const dtPoly* poly) const {
    return (excludedAreas & (std::uint64_t{1} << poly->getArea())) == 0;
}

} // namespace haylen::navigation3d
