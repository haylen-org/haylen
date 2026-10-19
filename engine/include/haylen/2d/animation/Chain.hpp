#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "haylen/math/Math.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::animation2d {

// A chain of points joined by segments of fixed lengths that procedural animation bends in two ways. Reaching keeps the first point at the root and bends the chain so its last point reaches a target, like a leg, an arm or a tentacle. Following moves the first point as a head and drags every other point behind it, like the body of a snake, a centipede or a tail. The bend between two segments never passes `maxBend`, and nothing allocates once the chain is made.
class Chain final {
  public:
    struct Options {
        math::Vec2 root;
        std::vector<float> lengths;
        // The direction the chain lies along from the root when it is made.
        float angle = 0.0F;
        float maxBend = math::Math::kPi;
    };

    struct Reach {
        // A point on the side the joints bend toward, such as above the knee of a leg.
        std::optional<math::Vec2> pole;
        int iterations = 10;
        float tolerance = 0.01F;
    };

    // Throws `std::invalid_argument` without segments or with a segment that is not longer than zero.
    explicit Chain(const Options& options);

    // Bends the chain from its root toward the target and returns whether its end reached it. A target out of reach stretches the chain straight toward it.
    bool reach(math::Vec2 target, const Reach& options);
    // Moves the first point to the head and drags each point after it to the length of its segment.
    void follow(math::Vec2 head);
    // Lays the chain straight from its root along the angle.
    void straighten(float angle);

    [[nodiscard]] std::size_t size() const noexcept {
        return points.size();
    }
    [[nodiscard]] math::Vec2 getRoot() const noexcept {
        return points.front();
    }
    // Moves the root and every point with it.
    void setRoot(math::Vec2 value) noexcept;
    [[nodiscard]] math::Vec2 getEnd() const noexcept {
        return points.back();
    }
    [[nodiscard]] std::span<const math::Vec2> getPoints() const noexcept {
        return points;
    }
    // Places one point without moving the others, such as a point of a body pushed out of a wall, and the next reach or follow restores the lengths from there. Throws `std::out_of_range` for an index past the end.
    void setPoint(std::size_t index, math::Vec2 value);
    [[nodiscard]] std::span<const float> getLengths() const noexcept {
        return lengths;
    }
    [[nodiscard]] float getTotalLength() const noexcept {
        return totalLength;
    }
    [[nodiscard]] float getMaxBend() const noexcept {
        return maxBend;
    }
    void setMaxBend(float value) noexcept;

  private:
    [[nodiscard]] static math::Vec2 directionOr(math::Vec2 value, math::Vec2 fallback) noexcept;
    [[nodiscard]] math::Vec2 limitBend(math::Vec2 direction, math::Vec2 previous) const noexcept;
    void pullToward(math::Vec2 target);
    void pushFromRoot(math::Vec2 root);
    void bendTowardPole(math::Vec2 target, math::Vec2 pole);

    std::vector<math::Vec2> points;
    std::vector<float> lengths;
    float totalLength = 0.0F;
    float maxBend = math::Math::kPi;
};

} // namespace haylen::animation2d
