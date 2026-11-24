#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace haylen::math {

// The basis functions of NURBS curves, surfaces and volumes along one parameter: the checks of a degree and its knots, the clamped uniform knots and the values of the basis functions of a span with their first and second derivatives.
class NurbsBasis final {
  public:
    static constexpr int kMaxDegree = 9;

    // The values of the degree plus one basis functions that do not vanish in a span, then their first and second derivatives.
    using Values = std::array<std::array<double, kMaxDegree + 1>, 3>;

    // Returns the knots that start and end with the degree plus one copies of a knot and spread the others one apart, so the curve reaches its first and last points.
    [[nodiscard]] static std::vector<float> makeClampedKnots(std::size_t count, int degree);

    // Throws `std::invalid_argument` for a degree outside 1 to 9, fewer points than the degree plus one, or knots that do not fit the points, with `subject` naming the curve, the surface or the volume without an article, such as `NURBS spline`.
    static void check(std::string_view subject, std::size_t count, int degree, std::span<const float> knots);

    // Returns the weights, or a weight of 1 for each point when there are none. Throws `std::invalid_argument` for weights that are not one finite number above 0 for each point, with `subject` naming what they weigh.
    [[nodiscard]] static std::vector<float> checkWeights(std::string_view subject, std::vector<float> weights, std::size_t count);

    // Returns the knot of the parameter `t` from 0 to 1, clamped to the range of the knots.
    [[nodiscard]] static float toKnot(std::span<const float> knots, int degree, std::size_t count, float t) noexcept;

    // Returns the span of knots whose piece holds `u`, the last piece for the end of the range.
    [[nodiscard]] static std::size_t findSpan(std::span<const float> knots, int degree, std::size_t count, float u) noexcept;

    // Evaluates the basis functions of the span at `u` and their derivatives up to `orders`, which is at most 2, by the algorithm of Cox and de Boor.
    static void evaluate(std::span<const float> knots, int degree, std::size_t span, float u, std::size_t orders, Values& values) noexcept;
};

} // namespace haylen::math
