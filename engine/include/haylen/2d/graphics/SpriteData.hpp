#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace haylen::graphics2d {

// What a sprite of a batch hands the GPU besides its quad: the texture of the batch it draws from, counted from 0, and four numbers that the material of the draw reads as `haylen_data`, such as a phase, a strength or a second color.
struct SpriteData {
    // A batch draws from up to this many textures in one draw call.
    static constexpr std::size_t kMaxTextures = 8;

    std::uint8_t texture = 0;
    std::array<float, 4> values{};

    [[nodiscard]] constexpr bool operator==(const SpriteData&) const noexcept = default;
};

} // namespace haylen::graphics2d
