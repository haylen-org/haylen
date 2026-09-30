#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::ai {

// A grid of influence over the world, such as the threat of enemies, the presence of allies or the pull of resources, which agents read to choose where to go. Cell (column, row) covers the square from `origin + (column, row) * cellSize`.
class InfluenceMap final {
  public:
    enum class Falloff : std::uint8_t {
        Constant,
        Linear,
        Quadratic,
    };

    // The value of a spot, at the center of its cell.
    struct Spot {
        math::Vec2 position{};
        float value = 0.0F;
    };

    // Throws `std::invalid_argument` for a side below 1 or a cell size that is not positive.
    InfluenceMap(int columns, int rows, float size, math::Vec2 corner = {});

    [[nodiscard]] static std::optional<Falloff> falloffFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view falloffName(Falloff value) noexcept;

    [[nodiscard]] int getColumns() const noexcept {
        return width;
    }
    [[nodiscard]] int getRows() const noexcept {
        return height;
    }
    [[nodiscard]] float getCellSize() const noexcept {
        return cellSize;
    }
    [[nodiscard]] math::Vec2 getOrigin() const noexcept {
        return origin;
    }
    [[nodiscard]] std::span<const float> getValues() const noexcept {
        return values;
    }

    // Throws `std::out_of_range` outside the map.
    [[nodiscard]] float get(int column, int row) const;
    void set(int column, int row, float value);

    // Returns the value at a world position, interpolated between cell centers, and 0 outside the map.
    [[nodiscard]] float sample(math::Vec2 position) const noexcept;

    // Adds `strength` to the cells whose centers lie within `radius` of the center, fading toward the edge.
    void stamp(math::Vec2 center, float strength, float radius, Falloff falloff = Falloff::Linear);

    // Spreads influence across the map: every cell moves toward the strongest influence of its eight neighbors, reduced by `e ^ (-decay * distance in cells)`. The argument `momentum` is the share of its old value a cell keeps.
    void propagate(float decay, float momentum);

    void scale(float factor) noexcept;
    // Adds another map of the same size times `weight`, such as allies minus enemies. Throws `std::invalid_argument` for maps of other sizes.
    void add(const InfluenceMap& other, float weight = 1.0F);
    void fill(float value) noexcept;

    // Returns the spot with the highest or lowest value among the cells whose centers lie within `radius` of the center, or nothing when no cell does.
    [[nodiscard]] std::optional<Spot> findHighest(math::Vec2 center, float radius) const noexcept;
    [[nodiscard]] std::optional<Spot> findLowest(math::Vec2 center, float radius) const noexcept;

    [[nodiscard]] math::Vec2 getCellCenter(int column, int row) const noexcept;

  private:
    static const std::array<std::pair<std::string_view, Falloff>, 3> kFalloffNames;

    struct Range {
        int firstColumn = 0;
        int firstRow = 0;
        int lastColumn = 0;
        int lastRow = 0;
    };

    // Returns the cells around a circle, clamped to the map.
    [[nodiscard]] Range rangeAround(math::Vec2 center, float radius) const noexcept;
    [[nodiscard]] std::size_t indexOf(int column, int row) const noexcept {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(width) + static_cast<std::size_t>(column);
    }
    [[nodiscard]] std::optional<Spot> findExtreme(math::Vec2 center, float radius, bool highest) const noexcept;

    int width;
    int height;
    float cellSize;
    math::Vec2 origin;
    std::vector<float> values;
    std::vector<float> scratch;
};

} // namespace haylen::ai
