#include "haylen/math/Box3.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace haylen::math {

Box3 Box3::bounds(std::span<const Vec3> points) {
    if (points.empty()) {
        throw std::invalid_argument("The bounds of points need at least one point.");
    }

    Box3 box{points.front(), points.front()};
    for (const Vec3 point : points.subspan(1)) {
        box = box.merged(point);
    }

    return box;
}

// Each element of the linear part spreads the box along one axis of the result, from the smaller to the larger of its two products with the corners, which gives the tightest box around the eight placed corners.
Box3 Box3::transformed(const Mat4& placement) const {
    if (!placement.isAffine()) {
        throw std::invalid_argument("A box is placed only by an affine matrix, whose last row is 0, 0, 0 and 1.");
    }

    const std::array<float, 3> low{min.x, min.y, min.z};
    const std::array<float, 3> high{max.x, max.y, max.z};
    std::array<float, 3> resultLow{placement.values[12], placement.values[13], placement.values[14]};
    std::array<float, 3> resultHigh = resultLow;
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t row = 0; row < 3; ++row) {
            const float element = placement.get(row, column);
            const float fromLow = element * low[column];
            const float fromHigh = element * high[column];
            resultLow[row] += std::min(fromLow, fromHigh);
            resultHigh[row] += std::max(fromLow, fromHigh);
        }
    }

    return {{resultLow[0], resultLow[1], resultLow[2]}, {resultHigh[0], resultHigh[1], resultHigh[2]}};
}

} // namespace haylen::math
