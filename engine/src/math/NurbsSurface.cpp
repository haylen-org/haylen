#include "haylen/math/NurbsSurface.hpp"

#include <array>
#include <stdexcept>

#include "math/NurbsBasis.hpp"

namespace haylen::math {

NurbsSurface::NurbsSurface(std::vector<Vec3> controlPoints, Options options) : points(std::move(controlPoints)), columns(options.columns), rows(columns > 0 ? points.size() / columns : 0), degreeU(options.degreeU), degreeV(options.degreeV) {
    if (columns == 0 || points.empty() || points.size() % columns != 0) {
        throw std::invalid_argument("A NURBS surface needs its points in whole rows of a number of columns above 0.");
    }

    weights = NurbsBasis::checkWeights("NURBS surface", std::move(options.weights), points.size());
    const bool fitsU = degreeU >= 1 && degreeU <= NurbsBasis::kMaxDegree && columns > static_cast<std::size_t>(degreeU);
    const bool fitsV = degreeV >= 1 && degreeV <= NurbsBasis::kMaxDegree && rows > static_cast<std::size_t>(degreeV);
    knotsU = options.knotsU.empty() && fitsU ? NurbsBasis::makeClampedKnots(columns, degreeU) : std::move(options.knotsU);
    knotsV = options.knotsV.empty() && fitsV ? NurbsBasis::makeClampedKnots(rows, degreeV) : std::move(options.knotsV);
    NurbsBasis::check("NURBS surface across its columns", columns, degreeU, knotsU);
    NurbsBasis::check("NURBS surface along its rows", rows, degreeV, knotsV);
}

Vec3 NurbsSurface::getPoint(float u, float v) const noexcept {
    return getDerivatives(u, v).point;
}

// The point is the ratio of the weighted points to the weights, and its derivatives along each parameter follow by the rule of quotients.
NurbsSurface::Derivatives NurbsSurface::getDerivatives(float u, float v) const noexcept {
    const float knotU = NurbsBasis::toKnot(knotsU, degreeU, columns, u);
    const float knotV = NurbsBasis::toKnot(knotsV, degreeV, rows, v);
    const std::size_t spanU = NurbsBasis::findSpan(knotsU, degreeU, columns, knotU);
    const std::size_t spanV = NurbsBasis::findSpan(knotsV, degreeV, rows, knotV);
    NurbsBasis::Values basisU{};
    NurbsBasis::Values basisV{};
    NurbsBasis::evaluate(knotsU, degreeU, spanU, knotU, 1, basisU);
    NurbsBasis::evaluate(knotsV, degreeV, spanV, knotV, 1, basisV);

    // The sums hold the weighted point, its derivative along u and along v, each with its weight in the last place.
    std::array<std::array<double, 4>, 3> sums{};
    for (std::size_t row = 0; row <= static_cast<std::size_t>(degreeV); ++row) {
        for (std::size_t column = 0; column <= static_cast<std::size_t>(degreeU); ++column) {
            const std::size_t index = (spanV - static_cast<std::size_t>(degreeV) + row) * columns + spanU - static_cast<std::size_t>(degreeU) + column;
            const std::array<double, 4> weighted{points[index].x * weights[index], points[index].y * weights[index], points[index].z * weights[index], weights[index]};
            const std::array<double, 3> factors{basisU[0][column] * basisV[0][row], basisU[1][column] * basisV[0][row], basisU[0][column] * basisV[1][row]};
            for (std::size_t sum = 0; sum < 3; ++sum) {
                for (std::size_t axis = 0; axis < 4; ++axis) {
                    sums[sum][axis] += factors[sum] * weighted[axis];
                }
            }
        }
    }

    const double weight = sums[0][3];
    const std::array<double, 3> point{sums[0][0] / weight, sums[0][1] / weight, sums[0][2] / weight};
    const double stretchU = static_cast<double>(knotsU[columns]) - knotsU[static_cast<std::size_t>(degreeU)];
    const double stretchV = static_cast<double>(knotsV[rows]) - knotsV[static_cast<std::size_t>(degreeV)];
    std::array<Vec3, 2> along{};
    for (std::size_t direction = 0; direction < 2; ++direction) {
        const std::array<double, 4>& sum = sums[direction + 1];
        const double stretch = direction == 0 ? stretchU : stretchV;
        along[direction] = {static_cast<float>((sum[0] - sum[3] * point[0]) / weight * stretch), static_cast<float>((sum[1] - sum[3] * point[1]) / weight * stretch), static_cast<float>((sum[2] - sum[3] * point[2]) / weight * stretch)};
    }

    return {.point = {static_cast<float>(point[0]), static_cast<float>(point[1]), static_cast<float>(point[2])}, .alongU = along[0], .alongV = along[1]};
}

Vec3 NurbsSurface::getNormal(float u, float v) const {
    const Derivatives derivatives = getDerivatives(u, v);
    const Vec3 normal = Vec3::cross(derivatives.alongU, derivatives.alongV);
    if (normal.isZero()) {
        throw std::invalid_argument("The NURBS surface folds onto a line or a point at these parameters, so it has no normal.");
    }

    return normal.getNormalized();
}

} // namespace haylen::math
