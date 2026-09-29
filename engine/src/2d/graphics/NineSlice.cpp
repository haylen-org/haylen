#include "haylen/2d/graphics/NineSlice.hpp"

#include <algorithm>
#include <utility>

namespace haylen::graphics2d {

const std::array<std::pair<std::string_view, NineSlice::Fill>, 2> NineSlice::kFillNames{{{"stretch", Fill::Stretch}, {"tile", Fill::Tile}}};

std::optional<NineSlice::Fill> NineSlice::fillFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kFillNames, name, &std::pair<std::string_view, Fill>::first);
    return found != kFillNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view NineSlice::fillName(Fill value) noexcept {
    return std::ranges::find(kFillNames, value, &std::pair<std::string_view, Fill>::second)->first;
}

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
