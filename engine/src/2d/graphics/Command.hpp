#pragma once

#include <cstdint>

#include "2d/graphics/Program.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {
struct TextureResource;
}

namespace haylen::graphics2d {

struct StaticBatchResource;

// One draw call: neighbouring draw items that share their state and continue each other's data.
struct Command {
    Program program = Program::Sprite;
    graphics::BlendMode::Type blend = graphics::BlendMode::Type::Alpha;
    std::uint32_t clip = 0;
    std::uint32_t shade = 0;
    graphics::TextureResource* texture = nullptr;
    StaticBatchResource* batch = nullptr;
    math::Vec2 offset{};
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

} // namespace haylen::graphics2d
