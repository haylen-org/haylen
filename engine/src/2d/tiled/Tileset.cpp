#include "haylen/2d/tiled/Tileset.hpp"

#include <stdexcept>

namespace haylen::tiled {

math::Rect Tileset::getSource(std::uint32_t tileId) const {
    if (const Tile* found = findTile(tileId); found != nullptr && !found->image.empty()) {
        return found->source;
    }
    if (columns <= 0) {
        throw std::out_of_range("The tileset has no tile \"" + std::to_string(tileId) + "\".");
    }
    const auto column = static_cast<float>(tileId % static_cast<std::uint32_t>(columns));
    const auto row = static_cast<float>(tileId / static_cast<std::uint32_t>(columns));
    return {static_cast<float>(margin) + column * (tileSize.x + static_cast<float>(spacing)), static_cast<float>(margin) + row * (tileSize.y + static_cast<float>(spacing)), tileSize.x, tileSize.y};
}

const graphics::Texture& Tileset::getTexture(std::uint32_t tileId) const {
    if (const Tile* found = findTile(tileId); found != nullptr && !found->image.empty()) {
        return found->texture;
    }
    return texture;
}

const Tile* Tileset::findTile(std::uint32_t tileId) const noexcept {
    const auto found = tiles.find(tileId);
    return found == tiles.end() ? nullptr : &found->second;
}

} // namespace haylen::tiled
