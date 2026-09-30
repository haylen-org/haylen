#include "haylen/2d/tiled/Map.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "2d/tiled/MapParser.hpp"

namespace haylen::tiled {

const std::array<std::pair<std::string_view, Map::Orientation>, 5> Map::kOrientationNames{{{"orthogonal", Orientation::Orthogonal}, {"isometric", Orientation::Isometric}, {"staggered", Orientation::Staggered}, {"hexagonal", Orientation::Hexagonal}, {"oblique", Orientation::Oblique}}};
const std::array<std::pair<std::string_view, Map::RenderOrder>, 4> Map::kRenderOrderNames{{{"right-down", RenderOrder::RightDown}, {"right-up", RenderOrder::RightUp}, {"left-down", RenderOrder::LeftDown}, {"left-up", RenderOrder::LeftUp}}};

std::optional<Map::Orientation> Map::orientationFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kOrientationNames, name, &std::pair<std::string_view, Orientation>::first);
    return found != kOrientationNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Map::orientationName(Orientation value) noexcept {
    return std::ranges::find(kOrientationNames, value, &std::pair<std::string_view, Orientation>::second)->first;
}

std::optional<Map::RenderOrder> Map::renderOrderFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kRenderOrderNames, name, &std::pair<std::string_view, RenderOrder>::first);
    return found != kRenderOrderNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Map::renderOrderName(RenderOrder value) noexcept {
    return std::ranges::find(kRenderOrderNames, value, &std::pair<std::string_view, RenderOrder>::second)->first;
}

Map Map::parse(const core::Json& document, std::string_view file, const JsonReader& read) {
    return MapParser::parse(document, file, read);
}

int Map::positiveModulo(int value, int divisor) noexcept {
    return ((value % divisor) + divisor) % divisor;
}

void Map::collectImages(const Layer& layer, std::vector<Image>& images) {
    if (layer.kind == Layer::Kind::Image && !layer.image.empty()) {
        images.push_back({layer.image, layer.transparentColor});
    }
    for (const Layer& child : layer.layers) {
        collectImages(child, images);
    }
}

void Map::attachLayer(Layer& layer, const TextureLoader& load) {
    if (layer.kind == Layer::Kind::Image && !layer.image.empty()) {
        layer.texture = load(layer.image);
    }
    for (Layer& child : layer.layers) {
        attachLayer(child, load);
    }
}

template <typename Layers> auto Map::findIn(Layers& candidates, std::string_view name) noexcept -> decltype(&candidates.front()) {
    for (auto& layer : candidates) {
        if (layer.name == name) {
            return &layer;
        }
        if (auto* found = findIn(layer.layers, name)) {
            return found;
        }
    }
    return nullptr;
}

std::vector<Map::Image> Map::getImages() const {
    std::vector<Image> images;
    for (const TilesetReference& reference : tilesets) {
        const Tileset& tileset = *reference.tileset;
        if (!tileset.image.empty()) {
            images.push_back({tileset.image, tileset.transparentColor});
        }
        for (const auto& [id, tile] : tileset.tiles) {
            if (!tile.image.empty()) {
                images.push_back({tile.image, tileset.transparentColor});
            }
        }
    }
    for (const Layer& layer : layers) {
        collectImages(layer, images);
    }

    std::set<std::string, std::less<>> seen;
    std::erase_if(images, [&seen](const Image& image) { return !seen.insert(image.path).second; });
    return images;
}

void Map::attachTextures(const TextureLoader& load) {
    for (TilesetReference& reference : tilesets) {
        Tileset& tileset = *reference.tileset;
        if (!tileset.image.empty()) {
            tileset.texture = load(tileset.image);
        }
        for (auto& [id, tile] : tileset.tiles) {
            if (!tile.image.empty()) {
                tile.texture = load(tile.image);
            }
        }
    }
    for (Layer& layer : layers) {
        attachLayer(layer, load);
    }
}

const Map::TilesetReference* Map::findTileset(std::uint32_t gid) const noexcept {
    const std::uint32_t id = tileId(gid);
    if (id == 0) {
        return nullptr;
    }
    const TilesetReference* found = nullptr;
    for (const TilesetReference& reference : tilesets) {
        if (reference.firstGid <= id) {
            found = &reference;
        }
    }
    if (found == nullptr) {
        return nullptr;
    }

    // Image collections keep the ids of the tiles removed from them unused, so their ids can reach past the tile count.
    const Tileset& tileset = *found->tileset;
    const std::uint32_t local = id - found->firstGid;
    const bool holds = tileset.image.empty() ? tileset.findTile(local) != nullptr : local < static_cast<std::uint32_t>(tileset.tileCount);
    return holds ? found : nullptr;
}

const Layer* Map::findLayer(std::string_view name) const noexcept {
    return findIn(layers, name);
}

Layer* Map::findLayer(std::string_view name) noexcept {
    return findIn(layers, name);
}

math::Vec2 Map::cellToWorld(int column, int row) const noexcept {
    const float tileWidth = tileSize.x;
    const float tileHeight = tileSize.y;
    switch (orientation) {
    case Orientation::Orthogonal:
        return {static_cast<float>(column) * tileWidth, static_cast<float>(row) * tileHeight};
    case Orientation::Isometric:
        return {static_cast<float>(column - row) * tileWidth * 0.5F + static_cast<float>(height) * tileWidth * 0.5F, static_cast<float>(column + row) * tileHeight * 0.5F};
    case Orientation::Oblique:
        return objectToWorld({static_cast<float>(column) * tileWidth, static_cast<float>(row) * tileHeight});
    case Orientation::Staggered:
    case Orientation::Hexagonal:
        break;
    }

    const auto side = static_cast<float>(orientation == Orientation::Hexagonal ? hexSideLength : 0);
    const int staggeredParity = staggerEven ? 0 : 1;
    if (staggerX) {
        const float columnWidth = (tileWidth + side) * 0.5F;
        const bool shifted = positiveModulo(column, 2) == staggeredParity;
        return {static_cast<float>(column) * columnWidth, static_cast<float>(row) * tileHeight + (shifted ? tileHeight * 0.5F : 0.0F)};
    }
    const float rowHeight = (tileHeight + side) * 0.5F;
    const bool shifted = positiveModulo(row, 2) == staggeredParity;
    return {static_cast<float>(column) * tileWidth + (shifted ? tileWidth * 0.5F : 0.0F), static_cast<float>(row) * rowHeight};
}

std::array<int, 2> Map::worldToCell(math::Vec2 position) const noexcept {
    const float tileWidth = tileSize.x;
    const float tileHeight = tileSize.y;
    if (orientation == Orientation::Orthogonal) {
        return {static_cast<int>(std::floor(position.x / tileWidth)), static_cast<int>(std::floor(position.y / tileHeight))};
    }
    if (orientation == Orientation::Isometric) {
        const float across = (position.x - static_cast<float>(height) * tileWidth * 0.5F) / (tileWidth * 0.5F);
        const float down = position.y / (tileHeight * 0.5F);
        return {static_cast<int>(std::floor((across + down) * 0.5F)), static_cast<int>(std::floor((down - across) * 0.5F))};
    }
    if (orientation == Orientation::Oblique) {
        // Undoes the shear, whose determinant parsing keeps away from zero.
        const float shearX = skew.x / tileHeight;
        const float shearY = skew.y / tileWidth;
        const float determinant = 1.0F - shearX * shearY;
        const math::Vec2 unskewed{(position.x - shearX * position.y) / determinant, (position.y - shearY * position.x) / determinant};
        return {static_cast<int>(std::floor(unskewed.x / tileWidth)), static_cast<int>(std::floor(unskewed.y / tileHeight))};
    }

    // Staggered and hexagonal cells are found by the nearest cell center, measured with the vertical axis scaled to match the horizontal one.
    const float aspect = tileWidth / tileHeight;
    const auto side = static_cast<float>(orientation == Orientation::Hexagonal ? hexSideLength : 0);
    const float columnStep = staggerX ? (tileWidth + side) * 0.5F : tileWidth;
    const float rowStep = staggerX ? tileHeight : (tileHeight + side) * 0.5F;
    const int guessColumn = static_cast<int>(std::floor(position.x / columnStep));
    const int guessRow = static_cast<int>(std::floor(position.y / rowStep));
    std::array<int, 2> best{guessColumn, guessRow};
    float bestDistance = std::numeric_limits<float>::max();
    for (int column = guessColumn - 2; column <= guessColumn + 2; ++column) {
        for (int row = guessRow - 2; row <= guessRow + 2; ++row) {
            const math::Vec2 center = cellToWorld(column, row) + tileSize * 0.5F;
            const math::Vec2 delta{position.x - center.x, (position.y - center.y) * aspect};
            if (const float distance = delta.getLengthSquared(); distance < bestDistance) {
                bestDistance = distance;
                best = {column, row};
            }
        }
    }
    return best;
}

math::Vec2 Map::objectToWorld(math::Vec2 position) const noexcept {
    if (orientation == Orientation::Oblique) {
        // Each row slides right by the horizontal skew and each column slides down by the vertical skew.
        return {position.x + position.y / tileSize.y * skew.x, position.y + position.x / tileSize.x * skew.y};
    }
    if (orientation != Orientation::Isometric) {
        return position;
    }
    // Isometric object coordinates measure both axes in tile heights along the diamond edges.
    const float across = position.x / tileSize.y;
    const float down = position.y / tileSize.y;
    return {(across - down) * tileSize.x * 0.5F + static_cast<float>(height) * tileSize.x * 0.5F, (across + down) * tileSize.y * 0.5F};
}

math::Rect Map::getPixelBounds() const noexcept {
    const auto columns = static_cast<float>(width);
    const auto rows = static_cast<float>(height);
    switch (orientation) {
    case Orientation::Orthogonal:
        return {0.0F, 0.0F, columns * tileSize.x, rows * tileSize.y};
    case Orientation::Isometric:
        return {0.0F, 0.0F, (columns + rows) * tileSize.x * 0.5F, (columns + rows) * tileSize.y * 0.5F};
    case Orientation::Oblique: {
        const math::Vec2 across = objectToWorld({columns * tileSize.x, 0.0F});
        const math::Vec2 down = objectToWorld({0.0F, rows * tileSize.y});
        const math::Vec2 corner = across + down;
        const math::Vec2 low{std::min({0.0F, across.x, down.x, corner.x}), std::min({0.0F, across.y, down.y, corner.y})};
        const math::Vec2 high{std::max({0.0F, across.x, down.x, corner.x}), std::max({0.0F, across.y, down.y, corner.y})};
        return {low.x, low.y, high.x - low.x, high.y - low.y};
    }
    case Orientation::Staggered:
    case Orientation::Hexagonal:
        break;
    }
    const auto side = static_cast<float>(orientation == Orientation::Hexagonal ? hexSideLength : 0);
    if (staggerX) {
        const float columnWidth = (tileSize.x + side) * 0.5F;
        return {0.0F, 0.0F, columns * columnWidth + (tileSize.x - columnWidth), rows * tileSize.y + tileSize.y * 0.5F};
    }
    const float rowHeight = (tileSize.y + side) * 0.5F;
    return {0.0F, 0.0F, columns * tileSize.x + tileSize.x * 0.5F, rows * rowHeight + (tileSize.y - rowHeight)};
}

} // namespace haylen::tiled
