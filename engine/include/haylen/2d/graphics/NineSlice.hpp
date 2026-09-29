#pragma once

#include <array>
#include <cstdint>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::graphics2d {

// Scalable frame made of nine source regions: three corners and edges on top, three in the middle and three at the bottom.
struct NineSlice {
    // How the edges and the center cover their part of the area.
    enum class Fill : std::uint8_t {
        Stretch,
        Tile,
    };

    graphics::Texture texture;
    std::array<math::Rect, 9> pieces{};
    Fill fill = Fill::Stretch;

    // Builds the nine regions by cutting one source rectangle with fixed borders.
    [[nodiscard]] static NineSlice fromBorders(graphics::Texture image, math::Rect source, math::Insets borders);
    [[nodiscard]] static NineSlice fromPieces(graphics::Texture image, const std::array<math::Rect, 9>& regions);

    [[nodiscard]] math::Insets getBorders() const noexcept;
    [[nodiscard]] bool isValid() const noexcept {
        return texture.isValid();
    }
};

} // namespace haylen::graphics2d
