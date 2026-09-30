#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/Explosion.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/math/Circle.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

class World;

// Destructible ground stored as a grid of samples from 0 (empty) to 255 (solid), which bitmaps and polygons fill and which shapes and explosions carve. The solid areas become chain loops on one static body per chunk, traced with marching squares, and `update` rebuilds only the chunks that changed. The terrain owns its bodies and must go before its world.
class Terrain final {
  public:
    // The sample `(column, row)` lies at `origin + (column, row) * cellSize`. Outlines are simplified to within `simplifyTolerance` world units, and chunks are `chunkSize` cells wide and tall.
    struct Options {
        int columns = 257;
        int rows = 129;
        float cellSize = 4.0F;
        math::Vec2 origin{};
        int chunkSize = 32;
        float simplifyTolerance = 1.0F;
        Shape::Options shape{};
    };

    // Starts empty. Throws `std::invalid_argument` for fewer than 2 samples on a side, a cell size or chunk size that is not positive, a negative tolerance or invalid shape options.
    Terrain(World& owner, const Options& settings);
    ~Terrain();

    Terrain(const Terrain&) = delete;
    Terrain& operator=(const Terrain&) = delete;

    [[nodiscard]] int getColumns() const noexcept {
        return options.columns;
    }
    [[nodiscard]] int getRows() const noexcept {
        return options.rows;
    }
    [[nodiscard]] float getCellSize() const noexcept {
        return options.cellSize;
    }
    [[nodiscard]] math::Rect getBounds() const noexcept;

    // Replaces every sample with values stored row by row, such as the alpha channel of an image. Throws `std::invalid_argument` when the count does not match.
    void setSamples(std::span<const std::uint8_t> values);
    [[nodiscard]] std::span<const std::uint8_t> getSamples() const noexcept {
        return samples;
    }
    // Throws `std::out_of_range` outside the grid.
    [[nodiscard]] std::uint8_t getSample(int column, int row) const;

    // Tells whether the point is inside the solid ground, interpolating between samples.
    [[nodiscard]] bool isSolid(math::Vec2 point) const noexcept;

    // Adds or removes material with soft edges, so outlines follow circles and polygons between samples.
    void fill(const math::Circle& circle);
    void fill(std::span<const math::Vec2> polygon);
    void carve(const math::Circle& circle);
    void carve(std::span<const math::Vec2> polygon);

    // Carves the crater and applies the blast to the bodies around it.
    std::vector<Explosion::Hit> explode(const math::Circle& crater, const Explosion::Options& blast);

    // Rebuilds the collision of the chunks changed since the last update and returns how many it rebuilt.
    std::size_t update();

    [[nodiscard]] std::size_t getChunkCount() const noexcept {
        return chunks.size();
    }
    [[nodiscard]] std::size_t getDirtyChunkCount() const noexcept;
    // Returns the collision outlines of every chunk as of the last update, in world units, which also draw the ground.
    [[nodiscard]] std::vector<std::vector<math::Vec2>> getOutlines() const;
    // Returns the bodies of the chunks that hold ground.
    [[nodiscard]] std::vector<Body> getBodies() const;

  private:
    struct Chunk {
        Body body;
        std::vector<std::vector<math::Vec2>> outlines;
        bool dirty = false;
    };

    struct Span {
        int firstColumn = 0;
        int firstRow = 0;
        int lastColumn = 0;
        int lastRow = 0;
    };

    // Returns the samples near the area, which a stamp may change.
    [[nodiscard]] Span spanAround(const math::Rect& area) const noexcept;
    void markDirty(const Span& span) noexcept;
    // Blends a shape into the samples, where `signedDistance` is negative inside the shape.
    template <typename Distance> void stamp(const math::Rect& area, bool adding, Distance&& signedDistance);
    [[nodiscard]] static float distanceToPolygon(std::span<const math::Vec2> polygon, math::Vec2 point) noexcept;

    void rebuild(std::size_t index);
    [[nodiscard]] Span spanOf(std::size_t index) const noexcept;
    // Simplifies an outline while keeping the points where it meets the chunk edge, so neighboring chunks still meet.
    [[nodiscard]] std::vector<math::Vec2> simplify(const std::vector<math::Vec2>& outline, const math::Rect& area) const;

    World& world;
    Options options;
    int chunkColumns = 1;
    int chunkRows = 1;
    std::vector<std::uint8_t> samples;
    std::vector<Chunk> chunks;
};

} // namespace haylen::physics2d
