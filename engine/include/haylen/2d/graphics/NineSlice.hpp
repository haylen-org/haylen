#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// Scalable frame made of nine source regions: three corners and edges on top, three in the middle and three at the bottom.
struct NineSlice {
    // How the edges and the center cover their part of the area.
    enum class Fill : std::uint8_t {
        Stretch,
        Tile,
    };

    // One quad of a drawn frame: the part of the area it covers and the region of the texture it shows, in texture pixels.
    struct Patch {
        math::Rect area;
        math::Rect source;
    };

    graphics::Texture texture;
    std::array<math::Rect, 9> pieces{};
    Fill fill = Fill::Stretch;

    // Builds the nine regions by cutting one source rectangle, the whole texture when it is empty, with fixed borders. Throws `std::invalid_argument` when a border is negative or the borders of an axis do not fit inside the source.
    [[nodiscard]] static NineSlice fromBorders(graphics::Texture image, math::Rect source, math::Insets borders);
    [[nodiscard]] static NineSlice fromPieces(graphics::Texture image, const std::array<math::Rect, 9>& regions);

    // Resolves the fill names `stretch` and `tile`.
    [[nodiscard]] static std::optional<Fill> fillFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view fillName(Fill value) noexcept;

    // The width of the widest piece of each side column and the height of the tallest piece of each side row.
    [[nodiscard]] math::Insets getBorders() const noexcept;
    [[nodiscard]] bool isValid() const noexcept {
        return texture.isValid();
    }

    // Cuts an area into the quads that draw the frame and appends them to `patches`. Corners keep their size times the border scale, and the borders of an axis shrink in proportion when the area is smaller than they are. Stretched edges and centers cover their part. Tiled edges repeat along their length, keeping the shape of their piece across the border, the tiled center repeats at the scaled size of its piece, and the last copy of a row or column is cropped. An area without a positive width and height adds nothing, and a border scale that is not positive throws `std::invalid_argument`.
    void layout(const math::Rect& area, float borderScale, std::vector<Patch>& patches) const;

  private:
    static const std::array<std::pair<std::string_view, Fill>, 2> kFillNames;

    // A last copy thinner than this fraction of a tile is a rounding error of the length, not a copy to draw.
    static constexpr float kSliver = 0.001F;

    static void tile(const math::Rect& area, const math::Rect& source, math::Vec2 size, std::vector<Patch>& patches);
    [[nodiscard]] static int countCopies(float length, float size) noexcept;
};

} // namespace haylen::graphics2d
