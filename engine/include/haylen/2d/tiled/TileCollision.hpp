#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

// The collision of one tile layer on one static body. The solid shapes of touching cells merge into one region whose outlines become chain loops, solid from the outside, so bodies slide along floors, walls and slopes without catching on the joints between tiles. Changing a cell traces again only the regions it touches.
class TileCollision final {
  public:
    // What a cell holds: whether its collision covers the whole cell, the closed outlines of its other solid shapes in world units, which merge with the cells they touch, and the shapes that stay apart on the body, such as sensors, lines and one-way platforms.
    struct Cell {
        bool full = false;
        std::vector<math::Polygon::Outline> outlines;
        std::vector<physics2d::Shape> shapes;
    };

    // Cell `(column, row)` covers the rectangle from `corner + (column, row) * size`, and the merged outlines collide with `shapeFilter`.
    TileCollision(physics2d::Body owner, std::uint32_t layerId, math::Vec2 corner, math::Vec2 size, const physics2d::CollisionFilter& shapeFilter);

    [[nodiscard]] std::uint32_t getLayerId() const noexcept {
        return layer;
    }
    [[nodiscard]] const physics2d::Body& getBody() const noexcept {
        return body;
    }
    [[nodiscard]] math::Vec2 getOrigin() const noexcept {
        return origin;
    }
    [[nodiscard]] const physics2d::CollisionFilter& getFilter() const noexcept {
        return filter;
    }
    [[nodiscard]] std::size_t getRegionCount() const noexcept {
        return regions.size() - freeRegions.size();
    }

    // Replaces what a cell holds, destroying the shapes it kept apart, and leaves the outlines around it for `update` to trace.
    void setCell(int column, int row, Cell value);

    // Traces the outlines of the regions whose cells changed since the last update into new chain loops.
    void update();

  private:
    struct Region {
        std::vector<std::int64_t> cells;
        std::vector<physics2d::Shape> shapes;
    };

    struct Stored {
        Cell cell;
        math::Rect bounds{};
    };

    // A side of a full cell that borders no other full cell, running clockwise on screen around the solid cells between two corners of the grid.
    struct Edge {
        std::int64_t start = 0;
        std::int64_t end = 0;
        int direction = 0;
        bool traced = false;
    };

    // Regions merge shapes whose bounds come this close, in world units.
    static constexpr float kTouch = 0.01F;
    // Points of a traced outline that stray less than this many world units from a straight line are dropped.
    static constexpr float kStraightness = 0.001F;

    // The steps along the grid of the sides of a cell in clockwise order on screen: east along the top, south along the right, west along the bottom and north along the left.
    static constexpr std::array<std::array<int, 2>, 4> kSteps{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};

    [[nodiscard]] static std::int64_t key(int column, int row) noexcept;
    [[nodiscard]] static int columnOf(std::int64_t cell) noexcept;
    [[nodiscard]] static int rowOf(std::int64_t cell) noexcept;
    [[nodiscard]] static bool isSolid(const Cell& cell) noexcept;

    [[nodiscard]] bool isFull(int column, int row) const noexcept;
    [[nodiscard]] math::Rect cellBounds(int column, int row) const noexcept;
    // Cells whose solid shapes may touch lie at most this many cells apart.
    [[nodiscard]] int getReach() const noexcept;
    void release(std::size_t index);
    void gather(std::int64_t seed);
    // Traces full cells into outlines that run clockwise on screen around solid ground and counterclockwise around holes.
    [[nodiscard]] std::vector<math::Polygon::Outline> traceFullCells(std::span<const std::int64_t> full) const;
    void build(Region& region);

    physics2d::Body body;
    std::uint32_t layer = 0;
    math::Vec2 origin{};
    math::Vec2 cellSize{};
    // The most whole cells any solid shape stands out of its own cell.
    int extent = 0;
    physics2d::CollisionFilter filter{};
    std::unordered_map<std::int64_t, Stored> cells;
    std::unordered_map<std::int64_t, std::size_t> regionOf;
    std::vector<Region> regions;
    std::vector<std::size_t> freeRegions;
    std::vector<std::int64_t> changed;
};

} // namespace haylen::tiled
