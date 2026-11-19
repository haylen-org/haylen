#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include "haylen/2d/tiled/Layer.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/2d/tiled/Property.hpp"
#include "haylen/2d/tiled/Tile.hpp"
#include "haylen/2d/tiled/Tileset.hpp"

namespace haylen::test {

// Finds the layers, tilesets, tiles and properties that a test expects a Tiled map to hold, and throws when one is missing, so a test never reads through an empty pointer.
class TiledHelper final {
  public:
    [[nodiscard]] static const tiled::Layer& layer(const tiled::Map& map, std::string_view name) {
        const tiled::Layer* found = map.findLayer(name);
        if (found == nullptr) {
            throw std::runtime_error("The map has no layer named \"" + std::string(name) + "\".");
        }

        return *found;
    }

    [[nodiscard]] static const tiled::Map::TilesetReference& tileset(const tiled::Map& map, std::uint32_t gid) {
        const tiled::Map::TilesetReference* found = map.findTileset(gid);
        if (found == nullptr) {
            throw std::runtime_error("No tileset of the map holds the tile " + std::to_string(gid) + ".");
        }

        return *found;
    }

    [[nodiscard]] static const tiled::Tile& tile(const tiled::Tileset& tileset, std::uint32_t id) {
        const tiled::Tile* found = tileset.findTile(id);
        if (found == nullptr) {
            throw std::runtime_error("The tileset has no tile " + std::to_string(id) + ".");
        }

        return *found;
    }

    [[nodiscard]] static const tiled::Property& property(const tiled::Properties& properties, std::string_view name) {
        const tiled::Property* found = properties.find(name);
        if (found == nullptr) {
            throw std::runtime_error("The properties have no \"" + std::string(name) + "\".");
        }

        return *found;
    }
};

} // namespace haylen::test
