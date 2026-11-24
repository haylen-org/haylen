#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>

namespace haylen::math {

// The table of arc lengths that the splines of both dimensions record at evenly spaced parameters, which turns a distance along a curve into its parameter.
class ArcLength final {
  public:
    // The most points that a sample along a curve takes.
    static constexpr std::size_t kMaxSteps = 16777216;

    // Returns the parameter from 0 to 1 at `distance` along the curve whose lengths the table holds, wrapping the distance around a closed curve and clamping it to an open one.
    [[nodiscard]] static float getParameter(std::span<const float> lengths, float distance, bool closed) noexcept {
        const float length = lengths.back();
        if (length <= 0.0F) {
            return 0.0F;
        }

        // Rounding can carry a wrapped distance just outside the curve, so it is clamped like the distance of an open curve.
        const float along = std::clamp(closed ? distance - std::floor(distance / length) * length : distance, 0.0F, length);
        const auto upper = std::upper_bound(lengths.begin(), lengths.end(), along);
        if (upper == lengths.end()) {
            return 1.0F;
        }

        const auto index = static_cast<std::size_t>(upper - lengths.begin());
        const float start = lengths[index - 1];
        const float span = lengths[index] - start;
        const float fraction = span > 0.0F ? (along - start) / span : 0.0F;
        return (static_cast<float>(index - 1) + fraction) / static_cast<float>(lengths.size() - 1);
    }

    // Returns how many points `spacing` apart fit along a curve of `length` from its start. Throws `std::invalid_argument` for a spacing that is not finite and above 0 or that takes more than `kMaxSteps` steps.
    [[nodiscard]] static std::size_t countSteps(float length, float spacing) {
        const double steps = std::ceil(static_cast<double>(length) / static_cast<double>(spacing));
        if (!std::isfinite(spacing) || spacing <= 0.0F || !(steps <= static_cast<double>(kMaxSteps))) {
            throw std::invalid_argument("Sampling by distance needs a finite spacing above 0 that takes at most 16777216 steps along the curve.");
        }

        return static_cast<std::size_t>(steps);
    }

    // Throws `std::invalid_argument` for a count of points above `kMaxSteps`.
    static void checkCount(std::size_t count) {
        if (count > kMaxSteps) {
            throw std::invalid_argument("Sampling a spline takes at most 16777216 points.");
        }
    }
};

} // namespace haylen::math
