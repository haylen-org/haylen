#pragma once

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::animation2d {

// One frame of a sprite sheet. Trimmed atlas frames remember where the trimmed rectangle sits inside the original frame, so pivots stay stable while playing.
struct SpriteFrame {
    math::Rect source{};
    math::Vec2 offset{};
    math::Vec2 originalSize{};
    float duration = 0.1F;
};

} // namespace haylen::animation2d
