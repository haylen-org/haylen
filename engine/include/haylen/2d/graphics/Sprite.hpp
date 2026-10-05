#pragma once

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/SpriteFlip.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// Immediate sprite draw. An empty size draws the source rectangle at its pixel size multiplied by the scale. The part colors recolor the sprite where the part mask of its order marks its parts.
struct Sprite {
    graphics::Texture texture;
    math::Rect source{};
    math::Vec2 position{};
    math::Vec2 size{};
    math::Vec2 scale{1.0F, 1.0F};
    math::Vec2 pivot{0.5F, 0.5F};
    float rotation = 0.0F;
    math::Color color = math::Color::white();
    math::Color flash = math::Color::transparent();
    SpriteFlip flip{};
    PartColors partColors{};
    DrawOrder order{};
};

} // namespace haylen::graphics2d
