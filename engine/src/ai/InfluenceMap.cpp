#include "haylen/ai/InfluenceMap.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "haylen/math/Math.hpp"

namespace haylen::ai {

InfluenceMap::InfluenceMap(int columns, int rows, float size, math::Vec2 corner) : width(columns), height(rows), cellSize(size), origin(corner) {
    if (columns < 1 || rows < 1 || size <= 0.0F) {
        throw std::invalid_argument("An influence map needs at least one cell on each side and a positive cell size.");
    }
    values.assign(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows), 0.0F);
}

std::optional<InfluenceMap::Falloff> InfluenceMap::falloffFromName(std::string_view name) noexcept {
    if (name == "constant") {
        return Falloff::Constant;
    }
    if (name == "linear") {
        return Falloff::Linear;
    }
    if (name == "quadratic") {
        return Falloff::Quadratic;
    }
    return std::nullopt;
}

std::string_view InfluenceMap::falloffName(Falloff value) noexcept {
    switch (value) {
    case Falloff::Constant:
        return "constant";
    case Falloff::Quadratic:
        return "quadratic";
    case Falloff::Linear:
        break;
    }
    return "linear";
}

float InfluenceMap::get(int column, int row) const {
    if (column < 0 || row < 0 || column >= width || row >= height) {
        throw std::out_of_range("The cell is outside the influence map.");
    }
    return values[indexOf(column, row)];
}

void InfluenceMap::set(int column, int row, float value) {
    if (column < 0 || row < 0 || column >= width || row >= height) {
        throw std::out_of_range("The cell is outside the influence map.");
    }
    values[indexOf(column, row)] = value;
}

math::Vec2 InfluenceMap::getCellCenter(int column, int row) const noexcept {
    return origin + math::Vec2{static_cast<float>(column) + 0.5F, static_cast<float>(row) + 0.5F} * cellSize;
}

float InfluenceMap::sample(math::Vec2 position) const noexcept {
    const math::Vec2 local = (position - origin) / cellSize;
    if (!(local.x >= 0.0F && local.y >= 0.0F && local.x <= static_cast<float>(width) && local.y <= static_cast<float>(height))) {
        return 0.0F;
    }

    // Between the outer cell centers and the map edge the value stays flat.
    const float x = std::clamp(local.x - 0.5F, 0.0F, static_cast<float>(width - 1));
    const float y = std::clamp(local.y - 0.5F, 0.0F, static_cast<float>(height - 1));
    const int column = std::min(static_cast<int>(x), std::max(0, width - 2));
    const int row = std::min(static_cast<int>(y), std::max(0, height - 2));
    const int right = std::min(column + 1, width - 1);
    const int below = std::min(row + 1, height - 1);
    const float across = x - static_cast<float>(column);
    const float down = y - static_cast<float>(row);
    const float top = math::Math::lerp(values[indexOf(column, row)], values[indexOf(right, row)], across);
    const float bottom = math::Math::lerp(values[indexOf(column, below)], values[indexOf(right, below)], across);
    return math::Math::lerp(top, bottom, down);
}

InfluenceMap::Range InfluenceMap::rangeAround(math::Vec2 center, float radius) const noexcept {
    const math::Vec2 local = (center - origin) / cellSize;
    const float reach = radius / cellSize;
    const auto columns = static_cast<float>(width);
    const auto rows = static_cast<float>(height);

    // Clamping while the bounds are still floats keeps huge and infinite ones from overflowing an `int`, and NaN ones give an empty range.
    return {static_cast<int>(std::fmin(std::fmax(std::floor(local.x - reach), 0.0F), columns)), static_cast<int>(std::fmin(std::fmax(std::floor(local.y - reach), 0.0F), rows)), static_cast<int>(std::fmin(std::fmax(std::ceil(local.x + reach), -1.0F), columns - 1.0F)), static_cast<int>(std::fmin(std::fmax(std::ceil(local.y + reach), -1.0F), rows - 1.0F))};
}

void InfluenceMap::stamp(math::Vec2 center, float strength, float radius, Falloff falloff) {
    if (radius <= 0.0F) {
        return;
    }
    const Range range = rangeAround(center, radius);

    for (int row = range.firstRow; row <= range.lastRow; ++row) {
        for (int column = range.firstColumn; column <= range.lastColumn; ++column) {
            const float distance = math::Vec2::distance(getCellCenter(column, row), center);
            if (distance > radius) {
                continue;
            }
            const float remaining = 1.0F - distance / radius;
            const float factor = falloff == Falloff::Constant ? 1.0F : (falloff == Falloff::Linear ? remaining : remaining * remaining);
            values[indexOf(column, row)] += strength * factor;
        }
    }
}

void InfluenceMap::propagate(float decay, float momentum) {
    const float straight = std::exp(-decay);
    const float diagonal = std::exp(-decay * std::numbers::sqrt2_v<float>);
    scratch.resize(values.size());

    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            float strongest = 0.0F;
            for (int offsetY = -1; offsetY <= 1; ++offsetY) {
                for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                    const int neighborColumn = column + offsetX;
                    const int neighborRow = row + offsetY;
                    if ((offsetX == 0 && offsetY == 0) || neighborColumn < 0 || neighborRow < 0 || neighborColumn >= width || neighborRow >= height) {
                        continue;
                    }
                    const float spread = values[indexOf(neighborColumn, neighborRow)] * (offsetX != 0 && offsetY != 0 ? diagonal : straight);
                    strongest = std::fabs(spread) > std::fabs(strongest) ? spread : strongest;
                }
            }
            const std::size_t index = indexOf(column, row);
            scratch[index] = math::Math::lerp(strongest, values[index], momentum);
        }
    }
    values.swap(scratch);
}

void InfluenceMap::scale(float factor) noexcept {
    for (float& value : values) {
        value *= factor;
    }
}

void InfluenceMap::add(const InfluenceMap& other, float weight) {
    if (other.width != width || other.height != height) {
        throw std::invalid_argument("Only influence maps of the same size add up.");
    }
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] += other.values[index] * weight;
    }
}

void InfluenceMap::fill(float value) noexcept {
    std::fill(values.begin(), values.end(), value);
}

std::optional<InfluenceMap::Spot> InfluenceMap::findExtreme(math::Vec2 center, float radius, bool highest) const noexcept {
    const Range range = rangeAround(center, radius);

    std::optional<Spot> found;
    for (int row = range.firstRow; row <= range.lastRow; ++row) {
        for (int column = range.firstColumn; column <= range.lastColumn; ++column) {
            const math::Vec2 position = getCellCenter(column, row);
            const float value = values[indexOf(column, row)];
            if (math::Vec2::distance(position, center) > radius) {
                continue;
            }
            if (!found || (highest ? value > found->value : value < found->value)) {
                found = Spot{position, value};
            }
        }
    }
    return found;
}

std::optional<InfluenceMap::Spot> InfluenceMap::findHighest(math::Vec2 center, float radius) const noexcept {
    return findExtreme(center, radius, true);
}

std::optional<InfluenceMap::Spot> InfluenceMap::findLowest(math::Vec2 center, float radius) const noexcept {
    return findExtreme(center, radius, false);
}

} // namespace haylen::ai
