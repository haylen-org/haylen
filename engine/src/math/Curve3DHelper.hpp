#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "haylen/math/CurveFrame.hpp"
#include "haylen/math/Spline3D.hpp"
#include "haylen/math/Vec3.hpp"
#include "math/ArcLength.hpp"
#include "math/ParallelTransport.hpp"

namespace haylen::math {

// The walks that the curves of 3D measured by arc length share: their samples, their closest point and their frames. A curve offers `getPoint`, `getTangent`, `getPointAtDistance`, `getTangentAtDistance`, `getLength` and `isClosed`, with parameters from 0 to 1, and `getDerivatives` for its closest point.
class Curve3DHelper final {
  public:
    // Returns the direction of travel from the derivatives, which at a point where the curve stops for an instant follows the way it leaves, or nothing where the curve stands still.
    [[nodiscard]] static std::optional<Vec3> getDirection(const Spline3D::Derivatives& derivatives) noexcept {
        if (!derivatives.first.isZero()) {
            return derivatives.first.getNormalized();
        }

        if (!derivatives.second.isZero()) {
            return derivatives.second.getNormalized();
        }

        return std::nullopt;
    }

    // Records the arc length at `steps` evenly spaced parameters after the start, which distance lookups interpolate.
    template <class Curve> static void measure(const Curve& curve, std::size_t steps, std::vector<float>& lengths) {
        lengths.assign(steps + 1, 0.0F);
        Vec3 previous = curve.getPoint(0.0F);
        for (std::size_t step = 1; step <= steps; ++step) {
            const Vec3 point = curve.getPoint(static_cast<float>(step) / static_cast<float>(steps));
            lengths[step] = lengths[step - 1] + Vec3::distance(previous, point);
            previous = point;
        }
    }

    // Fills the points spread evenly over the parameter, both ends included.
    template <class Curve> static void sample(const Curve& curve, std::span<Vec3> points) {
        const std::size_t count = points.size();
        for (std::size_t index = 0; index < count; ++index) {
            points[index] = curve.getPoint(count == 1 ? 0.0F : static_cast<float>(index) / static_cast<float>(count - 1));
        }
    }

    template <class Curve> [[nodiscard]] static std::vector<Vec3> sample(const Curve& curve, std::size_t count) {
        ArcLength::checkCount(count);
        std::vector<Vec3> result(count);
        sample(curve, std::span<Vec3>(result));
        return result;
    }

    template <class Curve> [[nodiscard]] static std::vector<Vec3> sampleEvenly(const Curve& curve, std::size_t count) {
        ArcLength::checkCount(count);
        std::vector<Vec3> result;
        result.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            const float share = count == 1 ? 0.0F : static_cast<float>(index) / static_cast<float>(count - 1);
            result.push_back(share >= 1.0F ? curve.getPoint(1.0F) : curve.getPointAtDistance(curve.getLength() * share));
        }

        return result;
    }

    template <class Curve> [[nodiscard]] static std::vector<Vec3> sampleByDistance(const Curve& curve, float spacing) {
        const std::size_t count = ArcLength::countSteps(curve.getLength(), spacing);
        std::vector<Vec3> result;
        result.reserve(count + 1);
        for (std::size_t index = 0; index < count; ++index) {
            result.push_back(curve.getPointAtDistance(static_cast<float>(index) * spacing));
        }

        if (!curve.isClosed()) {
            result.push_back(curve.getPoint(1.0F));
        }

        return result;
    }

    template <class Curve> [[nodiscard]] static std::vector<CurveFrame> getFrames(const Curve& curve, std::size_t count, Vec3 up) {
        if (count < 2) {
            throw std::invalid_argument("The frames of a curve need at least 2 frames, one at each end.");
        }

        ArcLength::checkCount(count);
        std::vector<CurveFrame> frames(count);
        for (std::size_t index = 0; index < count; ++index) {
            const bool last = index + 1 == count;
            const float distance = curve.getLength() * static_cast<float>(index) / static_cast<float>(count - 1);
            frames[index].position = last ? curve.getPoint(1.0F) : curve.getPointAtDistance(distance);
            frames[index].tangent = last ? curve.getTangent(1.0F) : curve.getTangentAtDistance(distance);
        }

        ParallelTransport::carry(frames, up, curve.isClosed());
        return frames;
    }

    // The nearest of `steps` evenly spaced points starts a search by Newton's method for the parameter where the line to the point stands at a right angle to the curve, kept between the neighbors of that point.
    template <class Curve> [[nodiscard]] static Spline3D::ClosestPoint findClosest(const Curve& curve, Vec3 point, std::size_t steps) {
        std::size_t nearest = 0;
        float nearestSquared = std::numeric_limits<float>::infinity();
        for (std::size_t step = 0; step <= steps; ++step) {
            const float squared = Vec3::distanceSquared(curve.getPoint(static_cast<float>(step) / static_cast<float>(steps)), point);
            if (squared < nearestSquared) {
                nearestSquared = squared;
                nearest = step;
            }
        }

        const bool closed = curve.isClosed();
        const float start = static_cast<float>(nearest) / static_cast<float>(steps);
        const float refined = refineClosest(curve, point, start, 1.0F / static_cast<float>(steps));
        const float t = closed ? refined - std::floor(refined) : refined;
        const Vec3 found = curve.getPoint(t);
        const float distance = Vec3::distance(found, point);
        if (distance * distance > nearestSquared) {
            return {.t = start, .point = curve.getPoint(start), .distance = std::sqrt(nearestSquared)};
        }

        return {.t = t, .point = found, .distance = distance};
    }

  private:
    static constexpr int kClosestIterations = 8;

    template <class Curve> [[nodiscard]] static float refineClosest(const Curve& curve, Vec3 point, float t, float step) {
        const bool closed = curve.isClosed();
        const float low = closed ? t - step : std::max(0.0F, t - step);
        const float high = closed ? t + step : std::min(1.0F, t + step);
        float current = t;
        for (int iteration = 0; iteration < kClosestIterations; ++iteration) {
            const Spline3D::Derivatives derivatives = curve.getDerivatives(current);
            const Vec3 offset = derivatives.point - point;
            const float slope = Vec3::dot(offset, derivatives.first);
            const float curvature = Vec3::dot(derivatives.first, derivatives.first) + Vec3::dot(offset, derivatives.second);
            if (!(curvature > 0.0F)) {
                break;
            }

            const float next = std::clamp(current - slope / curvature, low, high);
            const bool settled = std::fabs(next - current) <= 1e-7F;
            current = next;
            if (settled) {
                break;
            }
        }

        return current;
    }
};

} // namespace haylen::math
