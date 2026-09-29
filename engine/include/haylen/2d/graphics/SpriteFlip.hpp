#pragma once

namespace haylen::graphics2d {

// Mirrors a sprite horizontally, vertically and across its diagonal, in any combination. Tiled tiles combine the three to rotate.
struct SpriteFlip {
    bool horizontal = false;
    bool vertical = false;
    bool diagonal = false;

    [[nodiscard]] constexpr bool operator==(const SpriteFlip&) const noexcept = default;
};

} // namespace haylen::graphics2d
