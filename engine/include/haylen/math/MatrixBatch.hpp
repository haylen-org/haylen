#pragma once

#include <span>
#include <string_view>

#include "haylen/math/Mat4.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// Multiplies and applies arrays of matrices in one call, such as the world matrices of thousands of objects under one parent or the points of a mesh, with the vector instructions of the processor where the build has them. Each result matches the matrix math of `Mat4` within rounding.
class MatrixBatch final {
  public:
    // Returns the vector instructions that the batches run on in this build, `SSE2`, `Neon` or `WebAssembly SIMD`, or `scalar` on a processor without them.
    [[nodiscard]] static std::string_view getInstructions() noexcept;

    // Writes `parent * matrices[i]` into `results[i]`. The results may be the matrices themselves. Throws `std::invalid_argument` when the results hold another count.
    static void multiply(const Mat4& parent, std::span<const Mat4> matrices, std::span<Mat4> results);

    // Writes `lhs[i] * rhs[i]` into `results[i]`. The results may be either input. Throws `std::invalid_argument` when the three arrays hold different counts.
    static void multiplyPairs(std::span<const Mat4> lhs, std::span<const Mat4> rhs, std::span<Mat4> results);

    // Writes `matrix.apply(points[i])` into `results[i]`. The results may be the points themselves. Throws `std::invalid_argument` when the results hold another count.
    static void transformPoints(const Mat4& matrix, std::span<const Vec3> points, std::span<Vec3> results);
};

} // namespace haylen::math
