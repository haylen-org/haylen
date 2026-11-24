#include "haylen/math/NurbsVolume.hpp"

#include <array>
#include <stdexcept>

#include "math/NurbsBasis.hpp"

namespace haylen::math {

NurbsVolume::NurbsVolume(std::vector<Vec3> controlPoints, Options options) : points(std::move(controlPoints)), columns(options.columns), rows(options.rows), layers(columns > 0 && rows > 0 ? points.size() / (columns * rows) : 0), degreeU(options.degreeU), degreeV(options.degreeV), degreeW(options.degreeW) {
    if (columns == 0 || rows == 0 || points.empty() || points.size() % (columns * rows) != 0) {
        throw std::invalid_argument("A NURBS volume needs its points in whole layers of a number of columns and rows above 0.");
    }

    weights = NurbsBasis::checkWeights("NURBS volume", std::move(options.weights), points.size());
    const std::array<std::size_t, 3> counts{columns, rows, layers};
    const std::array<int, 3> degrees{degreeU, degreeV, degreeW};
    std::array<std::vector<float>*, 3> targets{&knotsU, &knotsV, &knotsW};
    std::array<std::vector<float>*, 3> given{&options.knotsU, &options.knotsV, &options.knotsW};
    const std::array<const char*, 3> subjects{"NURBS volume across its columns", "NURBS volume along its rows", "NURBS volume through its layers"};
    for (std::size_t direction = 0; direction < 3; ++direction) {
        const bool fits = degrees[direction] >= 1 && degrees[direction] <= NurbsBasis::kMaxDegree && counts[direction] > static_cast<std::size_t>(degrees[direction]);
        *targets[direction] = given[direction]->empty() && fits ? NurbsBasis::makeClampedKnots(counts[direction], degrees[direction]) : std::move(*given[direction]);
        NurbsBasis::check(subjects[direction], counts[direction], degrees[direction], *targets[direction]);
    }
}

Vec3 NurbsVolume::getPoint(float u, float v, float w) const noexcept {
    const float knotU = NurbsBasis::toKnot(knotsU, degreeU, columns, u);
    const float knotV = NurbsBasis::toKnot(knotsV, degreeV, rows, v);
    const float knotW = NurbsBasis::toKnot(knotsW, degreeW, layers, w);
    const std::size_t spanU = NurbsBasis::findSpan(knotsU, degreeU, columns, knotU);
    const std::size_t spanV = NurbsBasis::findSpan(knotsV, degreeV, rows, knotV);
    const std::size_t spanW = NurbsBasis::findSpan(knotsW, degreeW, layers, knotW);
    NurbsBasis::Values basisU{};
    NurbsBasis::Values basisV{};
    NurbsBasis::Values basisW{};
    NurbsBasis::evaluate(knotsU, degreeU, spanU, knotU, 0, basisU);
    NurbsBasis::evaluate(knotsV, degreeV, spanV, knotV, 0, basisV);
    NurbsBasis::evaluate(knotsW, degreeW, spanW, knotW, 0, basisW);

    std::array<double, 4> sum{};
    for (std::size_t layer = 0; layer <= static_cast<std::size_t>(degreeW); ++layer) {
        for (std::size_t row = 0; row <= static_cast<std::size_t>(degreeV); ++row) {
            for (std::size_t column = 0; column <= static_cast<std::size_t>(degreeU); ++column) {
                const std::size_t index = ((spanW - static_cast<std::size_t>(degreeW) + layer) * rows + spanV - static_cast<std::size_t>(degreeV) + row) * columns + spanU - static_cast<std::size_t>(degreeU) + column;
                const double factor = basisU[0][column] * basisV[0][row] * basisW[0][layer] * weights[index];
                sum[0] += factor * points[index].x;
                sum[1] += factor * points[index].y;
                sum[2] += factor * points[index].z;
                sum[3] += factor;
            }
        }
    }

    return {static_cast<float>(sum[0] / sum[3]), static_cast<float>(sum[1] / sum[3]), static_cast<float>(sum[2] / sum[3])};
}

} // namespace haylen::math
