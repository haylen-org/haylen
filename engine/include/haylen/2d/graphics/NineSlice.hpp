#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

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

    // Resolves the fill names `stretch` and `tile`.
    [[nodiscard]] static std::optional<Fill> fillFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view fillName(Fill value) noexcept;

    [[nodiscard]] math::Insets getBorders() const noexcept;
    [[nodiscard]] bool isValid() const noexcept {
        return texture.isValid();
    }

  private:
    static const std::array<std::pair<std::string_view, Fill>, 2> kFillNames;
};

} // namespace haylen::graphics2d
