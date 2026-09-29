#include "haylen/2d/graphics/NineSlice.hpp"

#include <utility>

namespace haylen::graphics2d {

NineSlice NineSlice::fromBorders(graphics::Texture image, math::Rect source, math::Insets borders) {
    if (source.isEmpty()) {
        source = {0.0F, 0.0F, image.getSize().x, image.getSize().y};
    }

    const float columns[4] = {source.getLeft(), source.getLeft() + borders.left, source.getRight() - borders.right, source.getRight()};
    const float rows[4] = {source.getTop(), source.getTop() + borders.top, source.getBottom() - borders.bottom, source.getBottom()};

    NineSlice slice;
    slice.texture = std::move(image);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            slice.pieces[static_cast<std::size_t>(row * 3 + column)] = math::Rect::fromMinMax(math::Vec2{columns[column], rows[row]}, math::Vec2{columns[column + 1], rows[row + 1]});
        }
    }
    return slice;
}

NineSlice NineSlice::fromPieces(graphics::Texture image, const std::array<math::Rect, 9>& regions) {
    NineSlice slice;
    slice.texture = std::move(image);
    slice.pieces = regions;
    return slice;
}

math::Insets NineSlice::getBorders() const noexcept {
    return {pieces[3].width, pieces[1].height, pieces[5].width, pieces[7].height};
}

} // namespace haylen::graphics2d
