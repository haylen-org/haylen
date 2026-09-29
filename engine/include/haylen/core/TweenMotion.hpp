#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "haylen/core/TweenValue.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {

// A shape that a tween property follows instead of a straight line between its start and end values. The end value of the property is the destination of jumps, paths and Bézier curves, the strength of shakes and punches, and the hidden value of blinks, which all come back to the start.
class TweenMotion final {
  public:
    // Makes the given number of hops on the way to the end, each rising power units toward negative y, which is up on screen.
    [[nodiscard]] static std::shared_ptr<TweenMotion> jump(float power, int hops);

    // Travels through the waypoints at constant speed, starting from the start value and ending at the end value, which must be the last waypoint. Smooth paths pass through every waypoint as a Catmull-Rom spline, and looping paths return to the start.
    [[nodiscard]] static std::shared_ptr<TweenMotion> path(std::vector<math::Vec2> waypoints, bool smooth, bool looping);

    // Follows a quadratic or cubic Bézier curve from the start to the end through one or two control points.
    [[nodiscard]] static std::shared_ptr<TweenMotion> bezier(std::vector<math::Vec2> controls);

    // Shakes around the start value vibrato times with a strength that fades out. Randomness in degrees, from 0 to 180, bends the direction of each shake of a vector, and the random seed makes the pattern repeatable.
    [[nodiscard]] static std::shared_ptr<TweenMotion> shake(int vibrato, float randomness, std::uint32_t randomSeed);

    // Springs toward the offset and back vibrato times, fading out. Elasticity, from 0 to 1, is how far it swings past the start on the way back.
    [[nodiscard]] static std::shared_ptr<TweenMotion> punch(int vibrato, float elasticity);

    // Switches between the start and the end value the given number of times and ends on the start value.
    [[nodiscard]] static std::shared_ptr<TweenMotion> blink(int times);

    // Returns the heading in radians of a path motion, for a rotation property that turns along it. The path must belong to a property of the same tween that begins first.
    [[nodiscard]] static std::shared_ptr<TweenMotion> orientation(std::shared_ptr<TweenMotion> followed);

    // Measures the geometry once the start and end values are known.
    void prepare(const TweenValue& from, const TweenValue& to);

    [[nodiscard]] TweenValue evaluate(const TweenValue& from, const TweenValue& to, float progress) const;

    // Returns the length of the travel, which speed-based tweens divide by their speed.
    [[nodiscard]] float getDistance(const TweenValue& from, const TweenValue& to) const;

  private:
    enum class Kind : std::uint8_t {
        Jump,
        Path,
        Bezier,
        Shake,
        Punch,
        Blink,
        Orientation,
    };

    // Samples per path segment for the table that maps distance to position.
    static constexpr int kPathSamples = 16;

    explicit TweenMotion(Kind value) noexcept : kind(value) {}

    [[nodiscard]] static float hash(std::uint32_t key, std::uint32_t index) noexcept;
    [[nodiscard]] math::Vec2 pathPoint(float t) const noexcept;
    [[nodiscard]] math::Vec2 pathAt(float progress) const noexcept;
    [[nodiscard]] float pathHeading(float progress) const noexcept;
    [[nodiscard]] TweenValue shakeOffset(const TweenValue& strength, float progress) const;
    [[nodiscard]] static math::Vec2 vector(const TweenValue& value);

    Kind kind;
    int count = 1;
    float amount = 0.0F;
    bool curved = false;
    bool closed = false;
    std::uint32_t seed = 0;
    std::vector<math::Vec2> points;
    std::vector<math::Vec2> nodes;
    std::vector<float> lengths;
    std::shared_ptr<TweenMotion> source;
};

} // namespace haylen::core
