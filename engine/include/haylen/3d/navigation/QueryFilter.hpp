#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace haylen::navigation3d {

// Which areas of a navigation mesh a query may cross and what each one costs. A path through an area costs its length times the cost of the area, so a cost of 4 on `water` makes an agent walk four meters around before it swims one. Every area costs 1 and none is excluded by default. Areas are the numbers that `NavMesh::getArea` gives their names.
class QueryFilter final {
  public:
    static constexpr std::size_t kAreaCount = 64;

    QueryFilter() noexcept;

    // Throws `std::invalid_argument` for area 0, which is never walked, an area past 63 or a cost that is not a finite number of at least 1. A cost below 1 would let the search guess the rest of a path too long, so its paths would no longer be the cheapest.
    void setCost(std::uint8_t area, float cost);
    [[nodiscard]] float getCost(std::uint8_t area) const noexcept {
        return costs[area & 0x3FU];
    }

    // Throws `std::invalid_argument` for area 0 or an area past 63.
    void setExcluded(std::uint8_t area, bool excluded);
    [[nodiscard]] bool isExcluded(std::uint8_t area) const noexcept {
        return (excludedAreas & (std::uint64_t{1} << (area & 0x3FU))) != 0;
    }

    [[nodiscard]] const std::array<float, kAreaCount>& getCosts() const noexcept {
        return costs;
    }

    friend bool operator==(const QueryFilter&, const QueryFilter&) = default;

  private:
    static void requireArea(std::uint8_t area);

    std::array<float, kAreaCount> costs{};
    std::uint64_t excludedAreas = 1;
};

} // namespace haylen::navigation3d
