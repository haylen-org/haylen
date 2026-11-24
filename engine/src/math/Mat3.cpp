#include "haylen/math/Mat3.hpp"

#include <stdexcept>

#include "haylen/math/Math.hpp"
#include "math/MatrixHelper.hpp"

namespace haylen::math {

bool Mat3::isInvertible() const noexcept {
    const float columnLengths = getColumn(0).getLength() * getColumn(1).getLength() * getColumn(2).getLength();
    return MatrixHelper::isInvertible(getDeterminant(), columnLengths);
}

// The inverse is the transposed matrix of the cross products of the columns, divided by the determinant.
Mat3 Mat3::getInverse() const {
    if (!isInvertible()) {
        throw std::invalid_argument("The matrix has no inverse, because it squashes space flat and its determinant is zero.");
    }

    const Vec3 x = getColumn(0);
    const Vec3 y = getColumn(1);
    const Vec3 z = getColumn(2);
    const float inverseDeterminant = 1.0F / getDeterminant();
    const Vec3 row0 = Vec3::cross(y, z) * inverseDeterminant;
    const Vec3 row1 = Vec3::cross(z, x) * inverseDeterminant;
    const Vec3 row2 = Vec3::cross(x, y) * inverseDeterminant;
    return fromColumns(row0, row1, row2).getTransposed();
}

bool Mat3::approximately(const Mat3& lhs, const Mat3& rhs, float epsilon) noexcept {
    for (std::size_t index = 0; index < lhs.values.size(); ++index) {
        if (!Math::approximately(lhs.values[index], rhs.values[index], epsilon)) {
            return false;
        }
    }

    return true;
}

} // namespace haylen::math
