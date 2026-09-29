#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/2d/spatial/Cell.hpp"

namespace haylen::spatial2d {

// A dense grid of integer cell values, such as terrain kinds or walls, for the grid algorithms of spatial2d. The algorithms treat zero as open ground and any other value as solid, unless they compare values.
class CellGrid final {
  public:
    CellGrid(int columns, int rows, std::int32_t value = 0);

    [[nodiscard]] int getWidth() const noexcept {
        return width;
    }
    [[nodiscard]] int getHeight() const noexcept {
        return height;
    }
    [[nodiscard]] bool contains(Cell cell) const noexcept {
        return cell.x >= 0 && cell.y >= 0 && cell.x < width && cell.y < height;
    }

    // Returns the value of a cell inside the grid, which callers check with contains first.
    [[nodiscard]] std::int32_t operator[](Cell cell) const noexcept {
        return values[indexOf(cell)];
    }

    [[nodiscard]] std::int32_t get(Cell cell) const;
    void set(Cell cell, std::int32_t value);
    void fill(std::int32_t value) noexcept;

    // Tells whether a cell is outside the grid or holds a non-zero value.
    [[nodiscard]] bool isSolid(Cell cell) const noexcept {
        return !contains(cell) || values[indexOf(cell)] != 0;
    }

    [[nodiscard]] std::span<const std::int32_t> getValues() const noexcept {
        return values;
    }
    [[nodiscard]] std::size_t indexOf(Cell cell) const noexcept {
        return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(cell.x);
    }

  private:
    void requireInside(Cell cell) const;

    int width;
    int height;
    std::vector<std::int32_t> values;
};

} // namespace haylen::spatial2d
