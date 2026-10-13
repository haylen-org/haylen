#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

class Renderer;

// A grid of cells built from code, such as a generated dungeon or a streamed world, that bakes into static batches by chunks of cells and draws them row by row from the top. Each cell shows a frame, a region of one of the textures of the layer, by its id, where 0 is empty. Chunks live in a hash map, so negative cells and sparse areas cost nothing where nothing was set, a chunk bakes again only on its first draw after one of its cells changed, and a draw leaves out the chunks outside the area the canvas shows.
class TileLayer final {
  public:
    struct Frame {
        std::size_t texture = 0;
        math::Rect source{};
        math::Vec2 offset{};

        // The size the frame draws at, or the size of its source when zero.
        math::Vec2 size{};
    };

    struct Options {
        math::Vec2 cellSize{32.0F, 32.0F};
        int chunkSize = 32;
    };

    static constexpr int kMaxFrame = 1 << 20;
    static constexpr int kMaxChunkSize = 256;

    // Throws `std::invalid_argument` without textures, or for a cell size that is not positive or a chunk size outside 1 to `kMaxChunkSize`.
    TileLayer(std::vector<graphics::Texture> images, Options value);

    // Defines or redefines a frame from 1 to `kMaxFrame`, and the cells that show it bake again.
    void defineFrame(int id, const Frame& frame);
    void setCell(int column, int row, int frame);

    // Sets the cells of a rectangle from rows of `width` frame ids, which changes each chunk it touches once.
    void setCells(int column, int row, int width, std::span<const float> ids);
    [[nodiscard]] int getCell(int column, int row) const noexcept;
    void clear() noexcept;

    // Draws the chunks in view, shifted by an offset in world units, which also gives parallax.
    void draw(Renderer& renderer, const DrawOrder& order = {}, math::Vec2 offset = {});

    [[nodiscard]] std::size_t getChunkCount() const noexcept {
        return chunks.size();
    }
    [[nodiscard]] std::size_t getBatchCount() const noexcept;
    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }

  private:
    struct Chunk {
        std::vector<std::int32_t> cells;
        std::vector<StaticSpriteBatch> batches;
        std::size_t filled = 0;
        bool stale = true;
    };

    [[nodiscard]] static std::uint64_t keyOf(int column, int row) noexcept;
    [[nodiscard]] static int floorDivide(int value, int divisor) noexcept;
    [[nodiscard]] static bool drawsBefore(std::uint64_t lhs, std::uint64_t rhs) noexcept;
    [[nodiscard]] math::Rect boundsOf(std::uint64_t key) const noexcept;
    void write(int column, int row, int frame);
    void bake(Renderer& renderer, std::uint64_t key, Chunk& chunk);

    std::vector<graphics::Texture> textures;
    Options options;
    std::vector<Frame> frames;
    std::vector<bool> defined;
    std::unordered_map<std::uint64_t, Chunk> chunks;

    // The chunks row by row from the top, the order they draw in, sorted again after chunks come or go.
    std::vector<std::uint64_t> drawOrder;
    bool orderChanged = false;

    // Reused by every bake, the sprites of one chunk for each texture.
    std::vector<std::vector<SpriteInstance>> pending;
};

} // namespace haylen::graphics2d
