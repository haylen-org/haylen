#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>

#include "haylen/2d/spatial/Cell.hpp"

namespace haylen::spatial2d {

// Computes what a viewer on a grid sees with Albert Ford's symmetric shadowcasting, the field of view of roguelikes and fog of war.
class FieldOfView final {
  public:
    // Calls reveal(cell) for every cell visible from the origin within the radius, measured between cell centers, including the origin and the opaque cells that bound the view. Opaque(cell) tells which cells block sight. Sight between transparent cells is symmetric, so when one sees another the other sees it back. Cells on the diagonals and axes of the origin may be revealed twice.
    template <typename Opaque, typename Reveal> static void compute(Cell origin, int radius, Opaque&& opaque, Reveal&& reveal) {
        if (radius < 0) {
            throw std::invalid_argument("A field of view needs a radius of zero or more cells.");
        }
        reveal(origin);
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            scan({.depth = 1, .start = {-1, 1}, .end = {1, 1}}, {.origin = origin, .quadrant = quadrant, .radius = radius}, opaque, reveal);
        }
    }

  private:
    // Slopes are exact fractions with a positive denominator, which keeps the view symmetric.
    struct Slope {
        std::int64_t numerator = 0;
        std::int64_t denominator = 1;
    };

    struct Row {
        int depth = 0;
        Slope start;
        Slope end;
    };

    struct Quadrant {
        Cell origin;
        int quadrant = 0;
        int radius = 0;

        [[nodiscard]] Cell transform(int depth, int column) const noexcept {
            switch (quadrant) {
            case 0:
                return {origin.x + column, origin.y - depth};
            case 1:
                return {origin.x + depth, origin.y + column};
            case 2:
                return {origin.x + column, origin.y + depth};
            default:
                return {origin.x - depth, origin.y + column};
            }
        }
    };

    [[nodiscard]] static std::int64_t floorDivide(std::int64_t dividend, std::int64_t divisor) noexcept {
        return dividend >= 0 ? dividend / divisor : -((-dividend + divisor - 1) / divisor);
    }

    // The first column of a row rounds the start slope half up, and the last column rounds the end slope half down.
    [[nodiscard]] static int firstColumn(const Row& row) noexcept {
        return static_cast<int>(floorDivide(2 * std::int64_t{row.depth} * row.start.numerator + row.start.denominator, 2 * row.start.denominator));
    }
    [[nodiscard]] static int lastColumn(const Row& row) noexcept {
        return static_cast<int>(-floorDivide(-(2 * std::int64_t{row.depth} * row.end.numerator - row.end.denominator), 2 * row.end.denominator));
    }

    // A transparent cell is only visible when its center lies inside the sector of the row.
    [[nodiscard]] static bool isCentered(const Row& row, int column) noexcept {
        return column * row.start.denominator >= row.depth * row.start.numerator && column * row.end.denominator <= row.depth * row.end.numerator;
    }

    [[nodiscard]] static Slope slopeOf(int depth, int column) noexcept {
        return {2 * std::int64_t{column} - 1, 2 * std::int64_t{depth}};
    }

    template <typename Opaque, typename Reveal> static void scan(Row row, const Quadrant& quadrant, Opaque& opaque, Reveal& reveal) {
        if (row.depth > quadrant.radius) {
            return;
        }

        std::optional<bool> previousOpaque;
        const int last = lastColumn(row);
        for (int column = firstColumn(row); column <= last; ++column) {
            const Cell cell = quadrant.transform(row.depth, column);
            const bool blocks = opaque(cell);
            const bool inRange = std::int64_t{row.depth} * row.depth + std::int64_t{column} * column <= std::int64_t{quadrant.radius} * quadrant.radius;
            if (inRange && (blocks || isCentered(row, column))) {
                reveal(cell);
            }

            // Sight resumes after a run of opaque cells, and a run of transparent cells that ends at an opaque one continues on the next row.
            if (previousOpaque == true && !blocks) {
                row.start = slopeOf(row.depth, column);
            }
            if (previousOpaque == false && blocks) {
                scan({.depth = row.depth + 1, .start = row.start, .end = slopeOf(row.depth, column)}, quadrant, opaque, reveal);
            }
            previousOpaque = blocks;
        }
        if (previousOpaque == false) {
            scan({.depth = row.depth + 1, .start = row.start, .end = row.end}, quadrant, opaque, reveal);
        }
    }
};

} // namespace haylen::spatial2d
