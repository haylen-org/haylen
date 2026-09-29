#pragma once

#include <cstdint>

#include "2d/graphics/Program.hpp"
#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {
struct TextureResource;
}

namespace haylen::graphics2d {

struct StaticBatchResource;

// One recorded draw. The items of a canvas are sorted by key and then by sequence before they become commands.
struct DrawItem {
    std::uint64_t key = 0;
    std::uint32_t sequence = 0;
    Program program = Program::Sprite;
    graphics::BlendMode::Type blend = graphics::BlendMode::Type::Alpha;
    std::uint16_t clip = 0;
    std::uint32_t shade = 0;
    graphics::TextureResource* texture = nullptr;
    StaticBatchResource* batch = nullptr;
    math::Vec2 offset{};
    std::uint32_t first = 0;
    std::uint32_t count = 0;

    // Orders draws by layer, shifted by the layer offset in effect, and inside a layer by depth or by the y the draw stands on, moved by its sort offset, when the canvas sorts that way.
    [[nodiscard]] static std::uint64_t makeKey(const DrawOrder& order, int layerOffset, Renderer::SortMode mode, float standingY) noexcept;

  private:
    [[nodiscard]] static std::uint32_t sortableFloat(float value) noexcept;
};

} // namespace haylen::graphics2d
