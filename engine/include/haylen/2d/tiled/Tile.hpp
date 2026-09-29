#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "haylen/2d/tiled/Object.hpp"
#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::tiled {

// A tile of a tileset that carries data of its own: a class, properties, an animation, collision shapes or, in image collections, its own image.
struct Tile {
    struct AnimationFrame {
        std::uint32_t tileId = 0;
        float duration = 0.1F;
    };

    std::uint32_t id = 0;
    std::string type;
    Properties properties;
    std::vector<AnimationFrame> animation;
    std::vector<Object> collision;
    std::string image;
    math::Rect source{};
    float probability = 1.0F;
    graphics::Texture texture;
};

} // namespace haylen::tiled
