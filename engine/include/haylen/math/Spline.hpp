#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// A smooth curve built from control points and measured by arc length, so it can be walked at a steady speed or sampled at even distances. Parameters run from 0 at the start to 1 at the end of the whole curve.
class Spline final {
  public:
    // Catmull-Rom curves pass through every point with centripetal parameterization. Bezier curves chain cubic segments whose points go end, control, control, end and so on, so open curves take 3n + 1 points and closed ones 3n. B-splines stay inside the hull of the points without passing through them, except for the ends of open curves.
    enum class Kind : std::uint8_t {
        CatmullRom,
        Bezier,
        BSpline,
    };

    // Throws `std::invalid_argument` when the kind needs more points or a different count.
    explicit Spline(std::vector<Vec2> controlPoints, Kind curveKind = Kind::CatmullRom, bool loop = false);

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;

    [[nodiscard]] const std::vector<Vec2>& getPoints() const noexcept {
        return points;
    }
    [[nodiscard]] Kind getKind() const noexcept {
        return kind;
    }
    [[nodiscard]] bool isClosed() const noexcept {
        return closed;
    }
    [[nodiscard]] std::size_t getSegmentCount() const noexcept {
        return segmentCount;
    }
    [[nodiscard]] float getLength() const noexcept {
        return lengths.back();
    }

    [[nodiscard]] Vec2 getPoint(float t) const noexcept;
    // Returns the unit direction of travel.
    [[nodiscard]] Vec2 getTangent(float t) const noexcept;
    [[nodiscard]] Vec2 getPointAtDistance(float distance) const noexcept;
    [[nodiscard]] Vec2 getTangentAtDistance(float distance) const noexcept;
    [[nodiscard]] float getParameterAtDistance(float distance) const noexcept;

    // Returns `count` points spread evenly over the parameter, both ends included.
    [[nodiscard]] std::vector<Vec2> sample(std::size_t count) const;
    // Returns points `spacing` apart along the curve from its start, plus the end point of open curves.
    [[nodiscard]] std::vector<Vec2> sampleByDistance(float spacing) const;

  private:
    static const std::array<std::pair<std::string_view, Kind>, 3> kKindNames;
    static constexpr std::size_t kStepsPerSegment = 32;
    static constexpr float kAlpha = 0.5F;

    [[nodiscard]] static std::size_t countSegments(std::size_t pointCount, Kind curveKind, bool loop);
    [[nodiscard]] Vec2 controlPoint(std::ptrdiff_t index) const noexcept;
    [[nodiscard]] Vec2 evaluate(std::size_t segment, float u) const noexcept;
    [[nodiscard]] Vec2 catmullRom(std::size_t segment, float u) const noexcept;
    [[nodiscard]] Vec2 bezier(std::size_t segment, float u) const noexcept;
    [[nodiscard]] Vec2 bSpline(std::size_t segment, float u) const noexcept;
    void measure();

    std::vector<Vec2> points;
    Kind kind;
    bool closed;
    std::size_t segmentCount;
    std::vector<float> lengths;
};

} // namespace haylen::math
