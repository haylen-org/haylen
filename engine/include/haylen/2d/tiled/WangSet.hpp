#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::tiled {

// A terrain set of a tileset, which the Tiled terrain brush paints with.
struct WangSet {
    struct Color {
        std::string name;
        std::string type;
        math::Color color;
        std::int32_t tile = -1;
        float probability = 1.0F;
        Properties properties;
    };

    struct Tile {
        std::uint32_t tileId = 0;
        std::array<std::uint8_t, 8> wangId{};
    };

    std::string name;
    std::string type;
    std::string kind;
    std::int32_t tile = -1;
    std::vector<Color> colors;
    std::vector<Tile> tiles;
    Properties properties;
};

} // namespace haylen::tiled
