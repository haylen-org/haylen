#include "haylen/2d/graphics/TileLayer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"

namespace haylen::graphics2d {

TileLayer::TileLayer(std::vector<graphics::Texture> images, Options value) : textures(std::move(images)), options(value) {
    if (textures.empty() || std::ranges::any_of(textures, [](const graphics::Texture& texture) { return !texture.isValid(); })) {
        throw std::invalid_argument("A tile layer needs at least one texture.");
    }

    if (!(options.cellSize.x > 0.0F && options.cellSize.y > 0.0F) || options.chunkSize < 1 || options.chunkSize > kMaxChunkSize) {
        throw std::invalid_argument("A tile layer needs a positive cell size and a chunk size from 1 to 256 cells.");
    }

    pending.resize(textures.size());
}

std::uint64_t TileLayer::keyOf(int column, int row) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(column)) << 32U) | static_cast<std::uint32_t>(row);
}

std::int32_t TileLayer::columnOf(std::uint64_t key) noexcept {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(key >> 32U));
}

std::int32_t TileLayer::rowOf(std::uint64_t key) noexcept {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(key));
}

bool TileLayer::drawsBefore(std::uint64_t lhs, std::uint64_t rhs) noexcept {
    return rowOf(lhs) != rowOf(rhs) ? rowOf(lhs) < rowOf(rhs) : columnOf(lhs) < columnOf(rhs);
}

// The arithmetic runs on 64 bits, so the cells at the ends of the range of a 32-bit integer find their chunks too.
std::int64_t TileLayer::floorDivide(std::int64_t value, std::int64_t divisor) noexcept {
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

TileLayer::Place TileLayer::locate(int column, int row) const noexcept {
    const std::int64_t size = options.chunkSize;
    const std::int64_t chunkColumn = floorDivide(column, size);
    const std::int64_t chunkRow = floorDivide(row, size);
    const std::int64_t cell = (row - chunkRow * size) * size + (column - chunkColumn * size);
    return {.chunk = keyOf(static_cast<int>(chunkColumn), static_cast<int>(chunkRow)), .cell = static_cast<std::size_t>(cell)};
}

math::Rect TileLayer::boundsOf(std::uint64_t key) const noexcept {
    const math::Vec2 size = options.cellSize * static_cast<float>(options.chunkSize);
    return {static_cast<float>(columnOf(key)) * size.x, static_cast<float>(rowOf(key)) * size.y, size.x, size.y};
}

void TileLayer::defineFrame(int id, const Frame& frame) {
    if (id < 1 || id > kMaxFrame) {
        throw std::invalid_argument("A frame of a tile layer has an id from 1 to 1048576, not " + std::to_string(id) + ".");
    }

    if (frame.texture >= textures.size()) {
        throw std::invalid_argument("A frame of a tile layer uses one of its " + std::to_string(textures.size()) + " textures.");
    }

    const auto slot = static_cast<std::size_t>(id);
    if (frames.size() <= slot) {
        frames.resize(slot + 1);
        defined.resize(slot + 1, false);
    }

    const bool redefined = defined[slot];
    frames[slot] = frame;
    defined[slot] = true;

    if (!redefined) {
        return;
    }

    for (auto& [key, chunk] : chunks) {
        chunk.stale = chunk.stale || std::ranges::find(chunk.cells, id) != chunk.cells.end();
    }
}

void TileLayer::write(int column, int row, int frame) {
    if (frame != 0 && (frame < 0 || static_cast<std::size_t>(frame) >= defined.size() || !defined[static_cast<std::size_t>(frame)])) {
        throw std::invalid_argument("The tile layer has no frame " + std::to_string(frame) + ".");
    }

    const Place place = locate(column, row);
    auto found = chunks.find(place.chunk);
    if (found == chunks.end()) {
        if (frame == 0) {
            return;
        }

        const auto cellCount = static_cast<std::size_t>(options.chunkSize * options.chunkSize);
        found = chunks.emplace(place.chunk, Chunk{.cells = std::vector<std::int32_t>(cellCount, 0)}).first;
        orderChanged = true;
    }

    Chunk& chunk = found->second;
    std::int32_t& cell = chunk.cells[place.cell];
    if (cell == frame) {
        return;
    }

    chunk.filled = chunk.filled - (cell != 0 ? 1 : 0) + (frame != 0 ? 1 : 0);
    cell = frame;
    chunk.stale = true;

    if (chunk.filled == 0) {
        chunks.erase(found);
        orderChanged = true;
    }
}

void TileLayer::setCell(int column, int row, int frame) {
    write(column, row, frame);
}

void TileLayer::setCells(int column, int row, int width, std::span<const float> ids) {
    if (width < 1 || ids.size() % static_cast<std::size_t>(width) != 0) {
        throw std::invalid_argument("The frames of a rectangle of a tile layer come in whole rows of its width.");
    }

    const auto rows = static_cast<std::int64_t>(ids.size() / static_cast<std::size_t>(width));
    if (column + std::int64_t{width} - 1 > std::numeric_limits<int>::max() || row + rows - 1 > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("A rectangle of a tile layer reaches past the cell 2147483647.");
    }

    if (!std::ranges::all_of(ids, [](float id) { return id >= 0.0F && id <= static_cast<float>(kMaxFrame) && id == std::floor(id); })) {
        throw std::invalid_argument("The frames of a rectangle of a tile layer are whole numbers from 0 to 1048576.");
    }

    for (std::size_t index = 0; index < ids.size(); ++index) {
        const int cellColumn = column + static_cast<int>(index % static_cast<std::size_t>(width));
        const int cellRow = row + static_cast<int>(index / static_cast<std::size_t>(width));
        write(cellColumn, cellRow, static_cast<int>(ids[index]));
    }
}

int TileLayer::getCell(int column, int row) const noexcept {
    const Place place = locate(column, row);
    const auto found = chunks.find(place.chunk);
    if (found == chunks.end()) {
        return 0;
    }

    return found->second.cells[place.cell];
}

void TileLayer::clear() noexcept {
    chunks.clear();
    drawOrder.clear();
}

std::size_t TileLayer::getBatchCount() const noexcept {
    std::size_t count = 0;
    for (const auto& [key, chunk] : chunks) {
        count += chunk.batches.size();
    }

    return count;
}

// A chunk holds one baked batch for each texture its cells use, each cell a sprite from the top-left corner of its cell moved by the offset of its frame.
void TileLayer::bake(Renderer& renderer, std::uint64_t key, Chunk& chunk) {
    for (std::vector<SpriteInstance>& sprites : pending) {
        sprites.clear();
    }

    const math::Vec2 origin = boundsOf(key).getMin();
    const int size = options.chunkSize;
    for (int index = 0; index < size * size; ++index) {
        const std::int32_t id = chunk.cells[static_cast<std::size_t>(index)];
        if (id == 0) {
            continue;
        }

        const Frame& frame = frames[static_cast<std::size_t>(id)];
        const math::Vec2 place = origin + math::Vec2{static_cast<float>(index % size), static_cast<float>(index / size)} * options.cellSize + frame.offset;
        pending[frame.texture].push_back({.position = place, .size = frame.size.isZero() ? frame.source.getSize() : frame.size, .source = frame.source, .pivot = {}});
    }

    chunk.batches.clear();

    for (std::size_t texture = 0; texture < textures.size(); ++texture) {
        if (!pending[texture].empty()) {
            chunk.batches.push_back(renderer.createStaticBatch(textures[texture], pending[texture]));
        }
    }

    chunk.stale = false;
}

void TileLayer::draw(Renderer& renderer, const DrawOrder& order, math::Vec2 offset) {
    if (orderChanged) {
        drawOrder.clear();
        for (const auto& [key, chunk] : chunks) {
            drawOrder.push_back(key);
        }

        std::ranges::sort(drawOrder, &TileLayer::drawsBefore);
        orderChanged = false;
    }

    const math::Rect view = renderer.getCanvasBounds().translated(-offset);
    for (const std::uint64_t key : drawOrder) {
        if (!boundsOf(key).intersects(view)) {
            continue;
        }

        Chunk& chunk = chunks.at(key);
        if (chunk.stale) {
            bake(renderer, key, chunk);
        }

        for (const StaticSpriteBatch& batch : chunk.batches) {
            renderer.drawStatic(batch, order, offset);
        }
    }
}

} // namespace haylen::graphics2d
