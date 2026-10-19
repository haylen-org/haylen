#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/2d/tiled/Tile.hpp"
#include "haylen/2d/tiled/WangSet.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

// A tileset embedded in a map or loaded from a .tsj file. Tiles are cut from one image, or each one has its own image in an image collection.
struct Tileset {
    // The grid Tiled places tile objects on in the editor, isometric for tiles drawn for isometric maps, and the size of its cells.
    struct Grid {
        bool isometric = false;
        math::Vec2 size{};
    };

    // How Wang painting may turn the tiles of the tileset, and whether it keeps the tiles as they are when it can.
    struct Transformations {
        bool flipHorizontal = false;
        bool flipVertical = false;
        bool rotate = false;
        bool preferUntransformed = false;
    };

    std::string name;
    std::string type;
    std::string path;
    std::string image;
    std::optional<math::Color> transparentColor;
    math::Vec2 imageSize{};
    math::Vec2 tileSize{};
    int columns = 0;
    int tileCount = 0;
    int margin = 0;
    int spacing = 0;
    math::Vec2 tileOffset{};
    std::string objectAlignment = "unspecified";
    bool renderGridSize = false;
    bool preserveAspect = false;
    Grid grid;
    Transformations transformations;
    Properties properties;
    std::map<std::uint32_t, Tile> tiles;
    std::vector<WangSet> wangSets;
    graphics::Texture texture;

    // Returns the source rectangle of a tile inside its texture.
    [[nodiscard]] math::Rect getSource(std::uint32_t tileId) const;
    [[nodiscard]] const graphics::Texture& getTexture(std::uint32_t tileId) const;
    [[nodiscard]] const Tile* findTile(std::uint32_t tileId) const noexcept;
};

} // namespace haylen::tiled
