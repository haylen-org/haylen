#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "haylen/math/Rect.hpp"
#include "haylen/ui/FocusDirection.hpp"

namespace haylen::ui {

// Picks the rectangle a directional move lands on, the way Android picks the next focused view: candidates in the beam of the source win, then the nearest one along the direction, with the distance along the direction weighing more than the distance across it.
class FocusSearch final {
  public:
    using Direction = FocusDirection;

    [[nodiscard]] static std::optional<std::size_t> find(const math::Rect& source, std::span<const math::Rect> candidates, Direction direction);

    // Moves the source just past the side of the bounds opposite to the direction, where a search that wraps around starts.
    [[nodiscard]] static math::Rect wrap(const math::Rect& source, const math::Rect& bounds, Direction direction);

  private:
    static constexpr float kMajorAxisWeight = 13.0F;

    [[nodiscard]] static bool isHorizontal(Direction direction) noexcept;
    [[nodiscard]] static bool isCandidate(const math::Rect& source, const math::Rect& target, Direction direction) noexcept;
    [[nodiscard]] static bool beamsOverlap(const math::Rect& source, const math::Rect& target, Direction direction) noexcept;
    [[nodiscard]] static bool isToDirectionOf(const math::Rect& source, const math::Rect& target, Direction direction) noexcept;
    [[nodiscard]] static float getMajorAxisDistance(const math::Rect& source, const math::Rect& target, Direction direction) noexcept;
    [[nodiscard]] static float getMajorAxisDistanceToFarEdge(const math::Rect& source, const math::Rect& target, Direction direction) noexcept;
    [[nodiscard]] static float getMinorAxisDistance(const math::Rect& source, const math::Rect& target, Direction direction) noexcept;
    [[nodiscard]] static bool beamBeats(const math::Rect& source, const math::Rect& first, const math::Rect& second, Direction direction) noexcept;
    [[nodiscard]] static bool isBetter(const math::Rect& source, const math::Rect& first, const math::Rect& second, Direction direction) noexcept;
};

} // namespace haylen::ui
