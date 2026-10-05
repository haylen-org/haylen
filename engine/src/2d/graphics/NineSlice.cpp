#include "haylen/2d/graphics/NineSlice.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
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
    if (!(borders.left >= 0.0F && borders.top >= 0.0F && borders.right >= 0.0F && borders.bottom >= 0.0F)) {
        throw std::invalid_argument("The borders of a nine-slice cannot be negative.");
    }
    if (borders.getHorizontal() > source.width || borders.getVertical() > source.height) {
        throw std::invalid_argument("The borders of a nine-slice must fit inside its source rectangle, with the left and right borders at most its width and the top and bottom borders at most its height.");
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
    return {
        .left = std::max({pieces[0].width, pieces[3].width, pieces[6].width}),
        .top = std::max({pieces[0].height, pieces[1].height, pieces[2].height}),
        .right = std::max({pieces[2].width, pieces[5].width, pieces[8].width}),
        .bottom = std::max({pieces[6].height, pieces[7].height, pieces[8].height}),
    };
}

void NineSlice::layout(const math::Rect& area, float borderScale, std::vector<Patch>& patches) const {
    if (!(borderScale > 0.0F && std::isfinite(borderScale))) {
        throw std::invalid_argument("A nine-slice border scale must be positive.");
    }
    if (area.isEmpty()) {
        return;
    }

    const math::Insets borders = getBorders();
    const float horizontal = borders.getHorizontal() * borderScale;
    const float vertical = borders.getVertical() * borderScale;
    const float scaleX = horizontal > area.width ? area.width / borders.getHorizontal() : borderScale;
    const float scaleY = vertical > area.height ? area.height / borders.getVertical() : borderScale;
    const std::array<float, 4> columns{area.getLeft(), area.getLeft() + borders.left * scaleX, area.getRight() - borders.right * scaleX, area.getRight()};
    const std::array<float, 4> rows{area.getTop(), area.getTop() + borders.top * scaleY, area.getBottom() - borders.bottom * scaleY, area.getBottom()};

    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            const math::Rect& source = pieces[row * 3 + column];
            const math::Rect part = math::Rect::fromMinMax(math::Vec2{columns[column], rows[row]}, math::Vec2{columns[column + 1], rows[row + 1]});
            if (source.isEmpty() || part.isEmpty()) {
                continue;
            }
            if (fill == Fill::Stretch || (row != 1 && column != 1)) {
                patches.push_back({.area = part, .source = source});
                continue;
            }

            // An edge repeats along its length at the scale its border gives its piece, and the center at the scale of the borders.
            const float width = column == 1 ? source.width * (row == 1 ? scaleX : part.height / source.height) : part.width;
            const float height = row == 1 ? source.height * (column == 1 ? scaleY : part.width / source.width) : part.height;
            tile(part, source, {width, height}, patches);
        }
    }
}

// Copies are placed by their index rather than by adding up their sizes, so the error of every addition never piles up into a gap or a sliver.
void NineSlice::tile(const math::Rect& area, const math::Rect& source, math::Vec2 size, std::vector<Patch>& patches) {
    const int columns = countCopies(area.width, size.x);
    const int rows = countCopies(area.height, size.y);
    for (int row = 0; row < rows; ++row) {
        const float top = area.y + size.y * static_cast<float>(row);
        const float height = std::min(size.y, area.getBottom() - top);
        for (int column = 0; column < columns; ++column) {
            const float left = area.x + size.x * static_cast<float>(column);
            const float width = std::min(size.x, area.getRight() - left);
            patches.push_back({.area = {left, top, width, height}, .source = {source.x, source.y, source.width * width / size.x, source.height * height / size.y}});
        }
    }
}

int NineSlice::countCopies(float length, float size) noexcept {
    return std::max(1, static_cast<int>(std::ceil(length / size - kSliver)));
}

} // namespace haylen::graphics2d
