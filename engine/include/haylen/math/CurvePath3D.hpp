#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/CurveFrame.hpp"
#include "haylen/math/Spline3D.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A path of curves walked one after another, such as a road of straight pieces and bends or a track made of several splines, measured by the length of all of them together. Distances run from the start of the first curve, and the parameter of the path runs from 0 at its start to 1 at its end by distance.
class CurvePath3D final {
  public:
    // How smoothly two curves join: in position alone, also in their direction, or also in their velocity.
    enum class Continuity : std::uint8_t {
        Position,
        Tangent,
        Velocity,
    };

    // The parameter of the closest point is the share of the length of the path at that point.
    using ClosestPoint = Spline3D::ClosestPoint;

    // How far apart two curves may end and start and still join, relative to the size of their coordinates.
    static constexpr float kJoinTolerance = 1e-4F;

    // Throws `std::invalid_argument` for a path without curves or with a curve that starts away from the end of the curve before it, or the first curve away from the end of the last when the path is closed.
    explicit CurvePath3D(std::vector<Spline3D> pathCurves, bool loop = false);

    [[nodiscard]] static std::optional<Continuity> continuityFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view continuityName(Continuity value) noexcept;

    [[nodiscard]] const std::vector<Spline3D>& getCurves() const noexcept {
        return curves;
    }
    [[nodiscard]] bool isClosed() const noexcept {
        return closed;
    }
    [[nodiscard]] float getLength() const noexcept {
        return starts.back();
    }
    // The joins in the order of the curves, the last one between the last curve and the first for a closed path.
    [[nodiscard]] const std::vector<Continuity>& getJoins() const noexcept {
        return joins;
    }

    [[nodiscard]] Vec3 getPoint(float t) const noexcept;
    // Throws `std::invalid_argument` where the path stands still, so it has no direction.
    [[nodiscard]] Vec3 getTangent(float t) const;
    [[nodiscard]] Vec3 getPointAtDistance(float distance) const noexcept;
    [[nodiscard]] Vec3 getTangentAtDistance(float distance) const;
    [[nodiscard]] ClosestPoint getClosestPoint(Vec3 point) const noexcept;

    // Returns `count` points spread evenly by distance along the path, both ends included. Throws `std::invalid_argument` for more than 16777216 points.
    [[nodiscard]] std::vector<Vec3> sampleEvenly(std::size_t count) const;
    // Returns points `spacing` apart along the path from its start, plus the end point of an open path. Throws `std::invalid_argument` for a spacing that is not finite and above 0 or that would take more than 16777216 steps along the path.
    [[nodiscard]] std::vector<Vec3> sampleByDistance(float spacing) const;
    // Returns `count` frames spread evenly by distance, as `Spline3D::getFrames` describes.
    [[nodiscard]] std::vector<CurveFrame> getFrames(std::size_t count, Vec3 up = Vec3::up()) const;

  private:
    static const std::array<std::pair<std::string_view, Continuity>, 3> kContinuityNames;

    [[nodiscard]] static Continuity measureJoin(const Spline3D& before, const Spline3D& after);
    [[nodiscard]] std::pair<std::size_t, float> locate(float distance) const noexcept;
    [[nodiscard]] float wrap(float distance) const noexcept;

    std::vector<Spline3D> curves;
    bool closed;
    std::vector<float> starts;
    std::vector<Continuity> joins;
};

} // namespace haylen::math
