#include "haylen/3d/navigation/QueryFilter.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace haylen::navigation3d {

QueryFilter::QueryFilter() noexcept {
    costs.fill(1.0F);
}

void QueryFilter::requireArea(std::uint8_t area) {
    if (area == 0 || area >= kAreaCount) {
        throw std::invalid_argument("A query filter takes the areas from 1 to 63, not " + std::to_string(area) + ".");
    }
}

void QueryFilter::setCost(std::uint8_t area, float cost) {
    requireArea(area);
    if (!std::isfinite(cost) || cost < 1.0F) {
        throw std::invalid_argument("The cost of an area of a query filter must be a finite number of at least 1.");
    }

    costs[area] = cost;
}

void QueryFilter::setExcluded(std::uint8_t area, bool excluded) {
    requireArea(area);
    const std::uint64_t bit = std::uint64_t{1} << area;
    excludedAreas = excluded ? excludedAreas | bit : excludedAreas & ~bit;
}

} // namespace haylen::navigation3d
