#include "haylen/math/MatrixBatch.hpp"

#include <stdexcept>

#include "math/BatchKernels.hpp"
#include "math/Float4.hpp"

namespace haylen::math {

std::string_view MatrixBatch::getInstructions() noexcept {
    return Float4::kInstructions;
}

void MatrixBatch::multiply(const Mat4& parent, std::span<const Mat4> matrices, std::span<Mat4> results) {
    if (results.size() != matrices.size()) {
        throw std::invalid_argument("A batch of matrices needs one result for each matrix.");
    }

    BatchKernels::multiply<Float4>(parent, matrices, results);
}

void MatrixBatch::multiplyPairs(std::span<const Mat4> lhs, std::span<const Mat4> rhs, std::span<Mat4> results) {
    if (rhs.size() != lhs.size() || results.size() != lhs.size()) {
        throw std::invalid_argument("A batch of pairs of matrices needs as many matrices on the right as on the left and one result for each pair.");
    }

    BatchKernels::multiplyPairs<Float4>(lhs, rhs, results);
}

void MatrixBatch::transformPoints(const Mat4& matrix, std::span<const Vec3> points, std::span<Vec3> results) {
    if (results.size() != points.size()) {
        throw std::invalid_argument("A batch of points needs one result for each point.");
    }

    BatchKernels::transformPoints<Float4>(matrix, points, results);
}

} // namespace haylen::math
