#pragma once

#include "haylen/2d/graphics/SpriteFlip.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// One quad of a batch. The source rectangle is in texture pixels, and an empty source covers the whole texture. The tilt lays the turned quad on a plane that leans away from the viewer by that angle in radians, which shortens its height on the screen by the cosine of the angle, so a ring on the ground spins as an ellipse.
struct SpriteInstance {
    math::Vec2 position{};
    math::Vec2 size{};
    math::Rect source{};
    math::Vec2 pivot{0.5F, 0.5F};
    float rotation = 0.0F;
    float tilt = 0.0F;
    math::Color color = math::Color::white();
    math::Color flash = math::Color::transparent();
    SpriteFlip flip{};
};

} // namespace haylen::graphics2d
