#pragma once

#include <cmath>

namespace haylen::math {

// The test of a singular matrix that `Mat3` and `Mat4` share.
class MatrixHelper final {
  public:
    // The smallest ratio of the determinant to the product of the column lengths of an invertible matrix. The ratio is one for a matrix whose columns stand at right angles and falls toward zero as the columns lean onto each other, so the test ignores the scale of the matrix.
    static constexpr float kSingularRatio = 1e-6F;

    [[nodiscard]] static bool isInvertible(float determinant, float columnLengths) noexcept {
        if (!std::isfinite(determinant) || !std::isfinite(columnLengths) || columnLengths == 0.0F) {
            return false;
        }

        return std::fabs(determinant) > kSingularRatio * columnLengths;
    }
};

} // namespace haylen::math
