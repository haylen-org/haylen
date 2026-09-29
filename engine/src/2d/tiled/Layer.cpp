#include "haylen/2d/tiled/Layer.hpp"

#include <stdexcept>

namespace haylen::tiled {

std::string_view Layer::kindName(Kind value) noexcept {
    switch (value) {
    case Kind::Tile:
        return "tile";
    case Kind::Object:
        return "object";
    case Kind::Image:
        return "image";
    case Kind::Group:
        return "group";
    }
    return "tile";
}

std::uint32_t Layer::getGid(int column, int row) const noexcept {
    if (chunks.empty()) {
        if (column < 0 || row < 0 || column >= width || row >= height) {
            return 0;
        }
        return gids[static_cast<std::size_t>(row * width + column)];
    }
    for (const Chunk& chunk : chunks) {
        if (column >= chunk.x && row >= chunk.y && column < chunk.x + chunk.width && row < chunk.y + chunk.height) {
            return chunk.gids[static_cast<std::size_t>((row - chunk.y) * chunk.width + (column - chunk.x))];
        }
    }
    return 0;
}

void Layer::setGid(int column, int row, std::uint32_t value) {
    if (chunks.empty()) {
        if (column < 0 || row < 0 || column >= width || row >= height) {
            throw std::out_of_range("The cell is outside the tile layer.");
        }
        gids[static_cast<std::size_t>(row * width + column)] = value;
        return;
    }
    for (Chunk& chunk : chunks) {
        if (column >= chunk.x && row >= chunk.y && column < chunk.x + chunk.width && row < chunk.y + chunk.height) {
            chunk.gids[static_cast<std::size_t>((row - chunk.y) * chunk.width + (column - chunk.x))] = value;
            return;
        }
    }
    throw std::out_of_range("The cell is outside every chunk of the infinite tile layer.");
}

} // namespace haylen::tiled
