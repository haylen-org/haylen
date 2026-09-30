#include "haylen/2d/navigation/Grid.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace haylen::navigation2d {

const std::array<std::pair<std::string_view, Grid::Topology>, 3> Grid::kTopologyNames{{{"square", Topology::Square}, {"hexagonal", Topology::Hexagonal}, {"staggered", Topology::Staggered}}};
const std::array<std::pair<std::string_view, Grid::Heuristic>, 4> Grid::kHeuristicNames{{{"manhattan", Heuristic::Manhattan}, {"octile", Heuristic::Octile}, {"euclidean", Heuristic::Euclidean}, {"chebyshev", Heuristic::Chebyshev}}};
const Grid::Layout Grid::kDefaultLayout{};

std::optional<Grid::Topology> Grid::topologyFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kTopologyNames, name, &std::pair<std::string_view, Topology>::first);
    return found != kTopologyNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Grid::topologyName(Topology value) noexcept {
    return std::ranges::find(kTopologyNames, value, &std::pair<std::string_view, Topology>::second)->first;
}

std::optional<Grid::Heuristic> Grid::heuristicFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kHeuristicNames, name, &std::pair<std::string_view, Heuristic>::first);
    return found != kHeuristicNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Grid::heuristicName(Heuristic value) noexcept {
    return std::ranges::find(kHeuristicNames, value, &std::pair<std::string_view, Heuristic>::second)->first;
}

Grid::Grid(int columns, int rows, const Layout& value) : width(columns), height(rows), layout(value) {
    if (columns <= 0 || rows <= 0 || static_cast<std::int64_t>(columns) * rows > std::numeric_limits<std::int32_t>::max()) {
        throw std::invalid_argument("A navigation grid needs a positive size that fits in 32-bit cell indices.");
    }
    const auto count = static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows);
    walkable.assign(count, 1);
    costs.assign(count, 1.0F);
}

void Grid::requireInside(Cell cell) const {
    if (!contains(cell)) {
        throw std::out_of_range("Cell " + std::to_string(cell.x) + "," + std::to_string(cell.y) + " is outside the navigation grid.");
    }
}

void Grid::setWalkable(Cell cell, bool value) {
    requireInside(cell);
    walkable[indexOf(cell)] = value ? 1 : 0;
}

void Grid::setCost(Cell cell, float value) {
    requireInside(cell);
    if (!(value >= 1.0F) || !std::isfinite(value)) {
        throw std::invalid_argument("A navigation cost must be finite and at least 1.");
    }

    float& cost = costs[indexOf(cell)];
    if (value != 1.0F && cost == 1.0F) {
        ++costlyCells;
    } else if (value == 1.0F && cost != 1.0F) {
        --costlyCells;
    }
    cost = value;
}

float Grid::getCost(Cell cell) const {
    requireInside(cell);
    return costs[indexOf(cell)];
}

Grid::Cell Grid::toAxial(Cell cell) const noexcept {
    const int parity = layout.staggerEven ? 1 : -1;
    if (layout.staggerX) {
        return {cell.x, cell.y - (cell.x + parity * (cell.x & 1)) / 2};
    }
    return {cell.x - (cell.y + parity * (cell.y & 1)) / 2, cell.y};
}

Grid::Cell Grid::fromAxial(Cell axial) const noexcept {
    const int parity = layout.staggerEven ? 1 : -1;
    if (layout.staggerX) {
        return {axial.x, axial.y + (axial.x + parity * (axial.x & 1)) / 2};
    }
    return {axial.x + (axial.y + parity * (axial.y & 1)) / 2, axial.y};
}

// A staggered cell sits half a cell further along when its row shifts. The lattice turns the diamonds by 45 degrees: u grows toward the lower right neighbor and v toward the lower left one, and the offset keeps every cell on whole lattice points.
Grid::Cell Grid::staggeredToLattice(int along, int row) const noexcept {
    const int shiftedParity = layout.staggerEven ? 0 : 1;
    const int shift = positiveModulo(row, 2) == shiftedParity ? 1 : 0;
    const int offset = 1 - shiftedParity;
    return {(2 * along + shift + row - offset) / 2, (row - 2 * along - shift - offset) / 2};
}

Grid::Cell Grid::staggeredFromLattice(Cell point) const noexcept {
    const int shiftedParity = layout.staggerEven ? 0 : 1;
    const int row = point.x + point.y + 1 - shiftedParity;
    const int shift = positiveModulo(row, 2) == shiftedParity ? 1 : 0;
    return {(point.x - point.y - shift) / 2, row};
}

Grid::Cell Grid::toLattice(Cell cell) const noexcept {
    if (layout.topology != Topology::Staggered) {
        return cell;
    }
    if (layout.staggerX) {
        const Cell point = staggeredToLattice(cell.y, cell.x);
        return {point.y, point.x};
    }
    return staggeredToLattice(cell.x, cell.y);
}

Grid::Cell Grid::fromLattice(Cell point) const noexcept {
    if (layout.topology != Topology::Staggered) {
        return point;
    }
    if (layout.staggerX) {
        const Cell cell = staggeredFromLattice({point.y, point.x});
        return {cell.y, cell.x};
    }
    return staggeredFromLattice(point);
}

float Grid::estimate(Cell from, Cell to, Heuristic heuristic) const noexcept {
    if (layout.topology == Topology::Hexagonal) {
        const Cell a = toAxial(from);
        const Cell b = toAxial(to);
        const int dq = b.x - a.x;
        const int dr = b.y - a.y;
        return static_cast<float>(std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) * 0.5F;
    }

    const Cell a = toLattice(from);
    const Cell b = toLattice(to);
    const auto dx = static_cast<float>(std::abs(b.x - a.x));
    const auto dy = static_cast<float>(std::abs(b.y - a.y));
    switch (heuristic) {
    case Heuristic::Manhattan:
        return dx + dy;
    case Heuristic::Octile:
        return std::max(dx, dy) + (kDiagonalStep - 1.0F) * std::min(dx, dy);
    case Heuristic::Euclidean:
        return std::hypot(dx, dy);
    case Heuristic::Chebyshev:
        break;
    }
    return std::max(dx, dy);
}

bool Grid::hasLineOfSight(Cell from, Cell to) const noexcept {
    if (!isWalkable(from) || !isWalkable(to)) {
        return false;
    }
    return layout.topology == Topology::Hexagonal ? hasHexLineOfSight(from, to) : hasLatticeLineOfSight(toLattice(from), toLattice(to));
}

bool Grid::hasLatticeLineOfSight(Cell from, Cell to) const noexcept {
    const int dx = std::abs(to.x - from.x);
    const int dy = std::abs(to.y - from.y);
    const int stepX = to.x > from.x ? 1 : -1;
    const int stepY = to.y > from.y ? 1 : -1;
    Cell point = from;

    // Walks the crossed cells in order by comparing where the segment meets the next vertical and horizontal cell borders, `(1 + 2 * x) / 2dx` against `(1 + 2 * y) / 2dy`.
    for (int x = 0, y = 0; x < dx || y < dy;) {
        const int decision = (1 + 2 * x) * dy - (1 + 2 * y) * dx;
        if (decision == 0) {
            // The segment passes exactly through a corner, which is only clear when both cells beside it are.
            if (!isWalkable(fromLattice({point.x + stepX, point.y})) || !isWalkable(fromLattice({point.x, point.y + stepY}))) {
                return false;
            }
            point = {point.x + stepX, point.y + stepY};
            ++x;
            ++y;
        } else if (decision < 0) {
            point.x += stepX;
            ++x;
        } else {
            point.y += stepY;
            ++y;
        }

        if (!isWalkable(fromLattice(point))) {
            return false;
        }
    }
    return true;
}

bool Grid::hasHexLineOfSight(Cell from, Cell to) const noexcept {
    const Cell a = toAxial(from);
    const Cell b = toAxial(to);
    const int steps = static_cast<int>(estimate(from, to, Heuristic::Chebyshev));

    // Samples the line between both centers in cube coordinates, nudged off the edges between hexagons so every sample rounds to one cell.
    for (int step = 1; step < steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        const float q = static_cast<float>(a.x) + static_cast<float>(b.x - a.x) * t + 1e-4F;
        const float r = static_cast<float>(a.y) + static_cast<float>(b.y - a.y) * t + 2e-4F;
        const float s = -q - r;
        float roundedQ = std::round(q);
        float roundedR = std::round(r);
        const float roundedS = std::round(s);
        const float errorQ = std::fabs(roundedQ - q);
        const float errorR = std::fabs(roundedR - r);
        const float errorS = std::fabs(roundedS - s);
        if (errorQ > errorR && errorQ > errorS) {
            roundedQ = -roundedR - roundedS;
        } else if (errorR > errorS) {
            roundedR = -roundedQ - roundedS;
        }
        if (!isWalkable(fromAxial({static_cast<int>(roundedQ), static_cast<int>(roundedR)}))) {
            return false;
        }
    }
    return true;
}

void Grid::smoothPath(std::vector<Cell>& path) const {
    if (path.size() <= 2) {
        return;
    }

    // The kept cells are a subsequence of the path, so they move down in place without ever overwriting a cell still to be read.
    std::size_t kept = 1;
    std::size_t anchor = 0;
    for (std::size_t next = 2; next < path.size(); ++next) {
        if (!hasLineOfSight(path[anchor], path[next])) {
            anchor = next - 1;
            path[kept++] = path[anchor];
        }
    }
    path[kept++] = path.back();
    path.resize(kept);
}

} // namespace haylen::navigation2d
