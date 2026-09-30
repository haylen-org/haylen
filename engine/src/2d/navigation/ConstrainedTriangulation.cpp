#include "2d/navigation/ConstrainedTriangulation.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <numeric>
#include <stdexcept>

namespace haylen::navigation2d {

int ConstrainedTriangulation::orient(const Point& a, const Point& b, const Point& c) noexcept {
    const double left = (a.x - c.x) * (b.y - c.y);
    const double right = (a.y - c.y) * (b.x - c.x);
    const double determinant = left - right;
    const double bound = kOrientErrorBound * (std::fabs(left) + std::fabs(right));
    if (determinant > bound) {
        return 1;
    }
    if (determinant < -bound) {
        return -1;
    }
    return exactOrient(a, b, c);
}

int ConstrainedTriangulation::exactOrient(const Point& a, const Point& b, const Point& c) noexcept {
    // The determinant expands into six products of coordinates, and each product is the sum of its rounded value and the error `fma` recovers.
    const std::array<std::pair<double, double>, 6> products{{{b.x, c.y}, {-b.x, a.y}, {-a.x, c.y}, {-b.y, c.x}, {b.y, a.x}, {a.y, c.x}}};
    std::array<double, 12> expansion{};
    std::size_t size = 0;
    for (const auto& [lhs, rhs] : products) {
        const double product = lhs * rhs;
        for (double term : {product, std::fma(lhs, rhs, -product)}) {
            // Adding a term to the expansion keeps every rounding error as a component, so the components sum to the determinant exactly.
            for (std::size_t index = 0; index < size; ++index) {
                const double sum = term + expansion[index];
                const double added = sum - term;
                expansion[index] = (term - (sum - added)) + (expansion[index] - added);
                term = sum;
            }
            expansion[size++] = term;
        }
    }

    // The components grow in magnitude and never overlap, so the largest one that is not zero carries the sign.
    for (std::size_t index = size; index-- > 0;) {
        if (expansion[index] != 0.0) {
            return expansion[index] > 0.0 ? 1 : -1;
        }
    }
    return 0;
}

bool ConstrainedTriangulation::inCircle(const Point& a, const Point& b, const Point& c, const Point& d) noexcept {
    const double adx = a.x - d.x;
    const double ady = a.y - d.y;
    const double bdx = b.x - d.x;
    const double bdy = b.y - d.y;
    const double cdx = c.x - d.x;
    const double cdy = c.y - d.y;
    const double aLift = adx * adx + ady * ady;
    const double bLift = bdx * bdx + bdy * bdy;
    const double cLift = cdx * cdx + cdy * cdy;
    const double determinant = aLift * (bdx * cdy - cdx * bdy) + bLift * (cdx * ady - adx * cdy) + cLift * (adx * bdy - bdx * ady);
    const double permanent = aLift * (std::fabs(bdx * cdy) + std::fabs(cdx * bdy)) + bLift * (std::fabs(cdx * ady) + std::fabs(adx * cdy)) + cLift * (std::fabs(adx * bdy) + std::fabs(bdx * ady));
    return determinant > kInCircleErrorBound * permanent;
}

bool ConstrainedTriangulation::crosses(std::int32_t a, std::int32_t b, std::int32_t c, std::int32_t d) const noexcept {
    return orient(pointAt(a), pointAt(b), pointAt(c)) * orient(pointAt(a), pointAt(b), pointAt(d)) < 0 && orient(pointAt(c), pointAt(d), pointAt(a)) * orient(pointAt(c), pointAt(d), pointAt(b)) < 0;
}

std::int32_t ConstrainedTriangulation::weld(const Point& point) {
    const auto qx = static_cast<std::int64_t>(std::floor(point.x / tolerance));
    const auto qy = static_cast<std::int64_t>(std::floor(point.y / tolerance));
    const auto keyOf = [](std::int64_t x, std::int64_t y) { return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) | static_cast<std::uint32_t>(y); };
    for (std::int64_t x = qx - 1; x <= qx + 1; ++x) {
        for (std::int64_t y = qy - 1; y <= qy + 1; ++y) {
            const auto found = weldIndex.find(keyOf(x, y));
            if (found != weldIndex.end() && std::hypot(pointAt(found->second).x - point.x, pointAt(found->second).y - point.y) <= 2.0 * tolerance) {
                return found->second;
            }
        }
    }
    points.push_back(point);
    const auto index = static_cast<std::int32_t>(points.size() - 1);
    weldIndex.try_emplace(keyOf(qx, qy), index);
    return index;
}

void ConstrainedTriangulation::intersect(std::size_t first, std::size_t second) {
    const Point p = pointAt(pieces[first].start);
    const Point r{pointAt(pieces[first].end).x - p.x, pointAt(pieces[first].end).y - p.y};
    const Point q = pointAt(pieces[second].start);
    const Point s{pointAt(pieces[second].end).x - q.x, pointAt(pieces[second].end).y - q.y};
    const double denominator = r.x * s.y - r.y * s.x;
    const Point offset{q.x - p.x, q.y - p.y};
    const auto isEnd = [this](std::size_t piece, std::int32_t vertex) { return pieces[piece].start == vertex || pieces[piece].end == vertex; };
    // clang-format off
    const auto cut = [&](std::size_t piece, double along, std::int32_t vertex) {
        if (!isEnd(piece, vertex)) {
            pieces[piece].cuts.emplace_back(along, vertex);
        }
    };
    // clang-format on

    const double scale = std::max({std::hypot(r.x, r.y), std::hypot(s.x, s.y), tolerance});
    if (std::fabs(denominator) > 1e-12 * scale * scale) {
        const double t = (offset.x * s.y - offset.y * s.x) / denominator;
        const double u = (offset.x * r.y - offset.y * r.x) / denominator;
        const double slack = 1e-9;
        if (t < -slack || t > 1.0 + slack || u < -slack || u > 1.0 + slack) {
            return;
        }
        const std::int32_t vertex = weld({p.x + r.x * t, p.y + r.y * t});
        cut(first, t, vertex);
        cut(second, u, vertex);
        return;
    }

    // Parallel segments only meet when they lie on one line, where each one is cut at the ends of the other that fall inside it.
    if (std::fabs(offset.x * r.y - offset.y * r.x) > tolerance * std::hypot(r.x, r.y)) {
        return;
    }
    const double lengthR = r.x * r.x + r.y * r.y;
    const double lengthS = s.x * s.x + s.y * s.y;
    for (const std::int32_t vertex : {pieces[second].start, pieces[second].end}) {
        const double t = ((pointAt(vertex).x - p.x) * r.x + (pointAt(vertex).y - p.y) * r.y) / lengthR;
        if (t > 0.0 && t < 1.0) {
            cut(first, t, vertex);
        }
    }
    for (const std::int32_t vertex : {pieces[first].start, pieces[first].end}) {
        const double u = ((pointAt(vertex).x - q.x) * s.x + (pointAt(vertex).y - q.y) * s.y) / lengthS;
        if (u > 0.0 && u < 1.0) {
            cut(second, u, vertex);
        }
    }
}

void ConstrainedTriangulation::prepare(std::span<const math::Segment> segments) {
    points.clear();
    pieces.clear();
    constraints.clear();
    weldIndex.clear();

    double extent = 1.0;
    for (const math::Segment& segment : segments) {
        for (const math::Vec2 end : {segment.start, segment.end}) {
            extent = std::max({extent, std::fabs(static_cast<double>(end.x)), std::fabs(static_cast<double>(end.y))});
        }
    }
    tolerance = extent * 1e-6;
    for (const math::Segment& segment : segments) {
        const std::int32_t start = weld({segment.start.x, segment.start.y});
        const std::int32_t end = weld({segment.end.x, segment.end.y});
        if (start != end) {
            pieces.push_back({.start = start, .end = end, .cuts = {}});
        }
    }

    // Sweeps the pieces from left to right, so only pieces whose horizontal extents overlap are tested against each other.
    const auto low = [this](std::size_t piece) { return std::min(pointAt(pieces[piece].start).x, pointAt(pieces[piece].end).x); };
    const auto high = [this](std::size_t piece) { return std::max(pointAt(pieces[piece].start).x, pointAt(pieces[piece].end).x); };
    const auto top = [this](std::size_t piece) { return std::min(pointAt(pieces[piece].start).y, pointAt(pieces[piece].end).y); };
    const auto bottom = [this](std::size_t piece) { return std::max(pointAt(pieces[piece].start).y, pointAt(pieces[piece].end).y); };
    std::vector<std::size_t> order(pieces.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(order, [&](std::size_t lhs, std::size_t rhs) { return low(lhs) < low(rhs); });
    for (std::size_t index = 0; index < order.size(); ++index) {
        for (std::size_t other = index + 1; other < order.size() && low(order[other]) <= high(order[index]) + tolerance; ++other) {
            if (top(order[other]) <= bottom(order[index]) + tolerance && top(order[index]) <= bottom(order[other]) + tolerance) {
                intersect(order[index], order[other]);
            }
        }
    }

    for (Piece& piece : pieces) {
        std::ranges::sort(piece.cuts);
        std::int32_t previous = piece.start;
        for (const auto& [along, vertex] : piece.cuts) {
            if (vertex != previous) {
                constraints.emplace_back(std::min(previous, vertex), std::max(previous, vertex));
                previous = vertex;
            }
        }
        if (previous != piece.end) {
            constraints.emplace_back(std::min(previous, piece.end), std::max(previous, piece.end));
        }
    }
    std::ranges::sort(constraints);
    constraints.erase(std::ranges::unique(constraints).begin(), constraints.end());
}

void ConstrainedTriangulation::setTriangle(std::int32_t index, std::array<std::int32_t, 3> corners, std::array<std::int32_t, 3> around, std::array<bool, 3> fixed) {
    triangles[static_cast<std::size_t>(index)] = {.vertices = corners, .neighbors = around, .constrained = fixed};
    for (const std::int32_t corner : corners) {
        vertexTriangles[static_cast<std::size_t>(corner)] = index;
    }
}

void ConstrainedTriangulation::replaceNeighbor(std::int32_t triangle, std::int32_t from, std::int32_t to) noexcept {
    if (triangle < 0) {
        return;
    }
    for (std::int32_t& neighbor : triangles[static_cast<std::size_t>(triangle)].neighbors) {
        if (neighbor == from) {
            neighbor = to;
            return;
        }
    }
}

int ConstrainedTriangulation::edgeIndex(std::int32_t triangle, std::int32_t from, std::int32_t to) const noexcept {
    const std::array<std::int32_t, 3>& corners = triangles[static_cast<std::size_t>(triangle)].vertices;
    for (int side = 0; side < 3; ++side) {
        if (corners[static_cast<std::size_t>(side)] == from && corners[static_cast<std::size_t>((side + 1) % 3)] == to) {
            return side;
        }
    }
    return -1;
}

int ConstrainedTriangulation::cornerIndex(std::int32_t triangle, std::int32_t vertex) const noexcept {
    const std::array<std::int32_t, 3>& corners = triangles[static_cast<std::size_t>(triangle)].vertices;
    return corners[0] == vertex ? 0 : (corners[1] == vertex ? 1 : 2);
}

void ConstrainedTriangulation::addSuperTriangle() {
    double left = points.front().x;
    double right = left;
    double lowest = points.front().y;
    double highest = lowest;
    for (const Point& point : points) {
        left = std::min(left, point.x);
        right = std::max(right, point.x);
        lowest = std::min(lowest, point.y);
        highest = std::max(highest, point.y);
    }
    const double size = std::max({right - left, highest - lowest, 1.0}) * 16.0;
    const Point center{(left + right) * 0.5, (lowest + highest) * 0.5};

    firstSuper = static_cast<std::int32_t>(points.size());
    points.push_back({center.x - 3.0 * size, center.y - size});
    points.push_back({center.x + 3.0 * size, center.y - size});
    points.push_back({center.x, center.y + 3.0 * size});
    vertexTriangles.assign(points.size(), 0);
    triangles.assign(1, {});
    kept.assign(1, false);
    setTriangle(0, {firstSuper, firstSuper + 1, firstSuper + 2}, {-1, -1, -1}, {});
    lastTriangle = 0;
}

std::int32_t ConstrainedTriangulation::locate(const Point& point) const {
    std::int32_t triangle = lastTriangle;
    for (int step = 0;; ++step) {
        const Triangle& current = triangles[static_cast<std::size_t>(triangle)];
        bool moved = false;
        // Starting from a different side each step keeps the walk from circling.
        for (int offset = 0; offset < 3 && !moved; ++offset) {
            const auto side = static_cast<std::size_t>((offset + step) % 3);
            if (orient(pointAt(current.vertices[side]), pointAt(current.vertices[(side + 1) % 3]), point) < 0) {
                triangle = current.neighbors[side];
                moved = true;
            }
        }
        if (!moved) {
            lastTriangle = triangle;
            return triangle;
        }
    }
}

void ConstrainedTriangulation::splitTriangle(std::int32_t triangle, std::int32_t vertex, std::vector<std::pair<std::int32_t, int>>& pending) {
    const Triangle old = triangles[static_cast<std::size_t>(triangle)];
    const auto [a, b, c] = old.vertices;
    const auto second = static_cast<std::int32_t>(triangles.size());
    const std::int32_t third = second + 1;
    triangles.resize(triangles.size() + 2);
    kept.resize(triangles.size(), kept[static_cast<std::size_t>(triangle)]);

    setTriangle(triangle, {a, b, vertex}, {old.neighbors[0], second, third}, {old.constrained[0], false, false});
    setTriangle(second, {b, c, vertex}, {old.neighbors[1], third, triangle}, {old.constrained[1], false, false});
    setTriangle(third, {c, a, vertex}, {old.neighbors[2], triangle, second}, {old.constrained[2], false, false});
    replaceNeighbor(old.neighbors[1], triangle, second);
    replaceNeighbor(old.neighbors[2], triangle, third);
    pending.insert(pending.end(), {{triangle, 0}, {second, 0}, {third, 0}});
}

void ConstrainedTriangulation::splitEdge(std::int32_t triangle, int side, std::int32_t vertex, std::vector<std::pair<std::int32_t, int>>& pending) {
    const Triangle old = triangles[static_cast<std::size_t>(triangle)];
    const auto index = static_cast<std::size_t>(side);
    const std::int32_t a = old.vertices[index];
    const std::int32_t b = old.vertices[(index + 1) % 3];
    const std::int32_t c = old.vertices[(index + 2) % 3];
    const std::int32_t other = old.neighbors[index];
    const bool fixed = old.constrained[index];

    // The point splits the triangle `a, b, c` into `p, b, c` and `a, p, c`, and the neighbor `b, a, d` into `p, a, d` and `b, p, d`.
    const auto first = static_cast<std::int32_t>(triangles.size());
    const Triangle opposite = triangles[static_cast<std::size_t>(other)];
    const auto otherSide = static_cast<std::size_t>(edgeIndex(other, b, a));
    const std::int32_t d = opposite.vertices[(otherSide + 2) % 3];
    const std::int32_t last = first + 1;
    triangles.resize(triangles.size() + 2);
    const bool keptTriangle = kept[static_cast<std::size_t>(triangle)];
    const bool keptOther = kept[static_cast<std::size_t>(other)];
    kept.push_back(keptTriangle);
    kept.push_back(keptOther);

    setTriangle(triangle, {vertex, b, c}, {last, old.neighbors[(index + 1) % 3], first}, {fixed, old.constrained[(index + 1) % 3], false});
    setTriangle(first, {a, vertex, c}, {other, triangle, old.neighbors[(index + 2) % 3]}, {fixed, false, old.constrained[(index + 2) % 3]});
    setTriangle(other, {vertex, a, d}, {first, opposite.neighbors[(otherSide + 1) % 3], last}, {fixed, opposite.constrained[(otherSide + 1) % 3], false});
    setTriangle(last, {b, vertex, d}, {triangle, other, opposite.neighbors[(otherSide + 2) % 3]}, {fixed, false, opposite.constrained[(otherSide + 2) % 3]});
    replaceNeighbor(old.neighbors[(index + 2) % 3], triangle, first);
    replaceNeighbor(opposite.neighbors[(otherSide + 2) % 3], other, last);
    pending.insert(pending.end(), {{triangle, 1}, {first, 2}, {other, 1}, {last, 2}});
}

void ConstrainedTriangulation::insertPoint(std::int32_t vertex) {
    const Point& point = pointAt(vertex);
    const std::int32_t triangle = locate(point);
    const Triangle& current = triangles[static_cast<std::size_t>(triangle)];

    std::vector<std::pair<std::int32_t, int>> pending;
    int onEdge = -1;
    for (int side = 0; side < 3; ++side) {
        if (orient(pointAt(current.vertices[static_cast<std::size_t>(side)]), pointAt(current.vertices[static_cast<std::size_t>((side + 1) % 3)]), point) == 0) {
            onEdge = side;
        }
    }
    if (onEdge >= 0) {
        splitEdge(triangle, onEdge, vertex, pending);
    } else {
        splitTriangle(triangle, vertex, pending);
    }
    legalize(pending);
}

std::pair<std::int32_t, std::int32_t> ConstrainedTriangulation::flip(std::int32_t triangle, int side) {
    const Triangle old = triangles[static_cast<std::size_t>(triangle)];
    const auto index = static_cast<std::size_t>(side);
    const std::int32_t a = old.vertices[index];
    const std::int32_t b = old.vertices[(index + 1) % 3];
    const std::int32_t c = old.vertices[(index + 2) % 3];
    const std::int32_t other = old.neighbors[index];
    const Triangle opposite = triangles[static_cast<std::size_t>(other)];
    const auto otherSide = static_cast<std::size_t>(edgeIndex(other, b, a));
    const std::int32_t d = opposite.vertices[(otherSide + 2) % 3];

    // The triangles `a, b, c` and `b, a, d` become `a, d, c` and `d, b, c`, which share the diagonal between `c` and `d`.
    setTriangle(triangle, {a, d, c}, {opposite.neighbors[(otherSide + 1) % 3], other, old.neighbors[(index + 2) % 3]}, {opposite.constrained[(otherSide + 1) % 3], false, old.constrained[(index + 2) % 3]});
    setTriangle(other, {d, b, c}, {opposite.neighbors[(otherSide + 2) % 3], old.neighbors[(index + 1) % 3], triangle}, {opposite.constrained[(otherSide + 2) % 3], old.constrained[(index + 1) % 3], false});
    replaceNeighbor(opposite.neighbors[(otherSide + 1) % 3], other, triangle);
    replaceNeighbor(old.neighbors[(index + 1) % 3], triangle, other);
    return {triangle, other};
}

void ConstrainedTriangulation::legalize(std::vector<std::pair<std::int32_t, int>>& pending) {
    while (!pending.empty()) {
        const auto [triangle, side] = pending.back();
        pending.pop_back();
        const Triangle& current = triangles[static_cast<std::size_t>(triangle)];
        const auto index = static_cast<std::size_t>(side);
        const std::int32_t other = current.neighbors[index];
        if (other < 0 || current.constrained[index]) {
            continue;
        }

        const std::int32_t a = current.vertices[index];
        const std::int32_t b = current.vertices[(index + 1) % 3];
        const std::int32_t c = current.vertices[(index + 2) % 3];
        const auto otherSide = static_cast<std::size_t>(edgeIndex(other, b, a));
        const std::int32_t d = triangles[static_cast<std::size_t>(other)].vertices[(otherSide + 2) % 3];
        if (!inCircle(pointAt(a), pointAt(b), pointAt(c), pointAt(d))) {
            continue;
        }

        // Both new triangles keep the vertex `c`, and their sides across from it are the ones that may now break the Delaunay rule.
        const auto [first, second] = flip(triangle, side);
        pending.emplace_back(first, 0);
        pending.emplace_back(second, 0);
    }
}

std::int32_t ConstrainedTriangulation::findEdge(std::int32_t from, std::int32_t to) const {
    const std::int32_t start = vertexTriangles[static_cast<std::size_t>(from)];
    std::int32_t triangle = start;
    do {
        const auto corner = static_cast<std::size_t>(cornerIndex(triangle, from));
        const Triangle& current = triangles[static_cast<std::size_t>(triangle)];
        if (current.vertices[(corner + 1) % 3] == to) {
            return triangle;
        }
        triangle = current.neighbors[(corner + 2) % 3];
    } while (triangle >= 0 && triangle != start);
    if (triangle == start) {
        return -1;
    }

    // The fan hit the outer boundary, so the rest of it lies the other way around the vertex.
    triangle = triangles[static_cast<std::size_t>(start)].neighbors[static_cast<std::size_t>(cornerIndex(start, from))];
    while (triangle >= 0) {
        const auto corner = static_cast<std::size_t>(cornerIndex(triangle, from));
        const Triangle& current = triangles[static_cast<std::size_t>(triangle)];
        if (current.vertices[(corner + 1) % 3] == to) {
            return triangle;
        }
        triangle = current.neighbors[corner];
    }
    return -1;
}

void ConstrainedTriangulation::setConstrained(std::int32_t from, std::int32_t to) {
    for (const auto& [start, end] : {Edge{from, to}, Edge{to, from}}) {
        const std::int32_t triangle = findEdge(start, end);
        if (triangle >= 0) {
            triangles[static_cast<std::size_t>(triangle)].constrained[static_cast<std::size_t>(edgeIndex(triangle, start, end))] = true;
        }
    }
}

bool ConstrainedTriangulation::collectCrossings(std::int32_t from, std::int32_t to, std::deque<Edge>& crossed, std::int32_t& onSegment) const {
    const Point& a = pointAt(from);
    const Point& b = pointAt(to);
    // clang-format off
    const auto between = [&](std::int32_t vertex) {
        const Point& point = pointAt(vertex);
        return orient(a, b, point) == 0 && (point.x - a.x) * (b.x - a.x) + (point.y - a.y) * (b.y - a.y) > 0.0;
    };
    // clang-format on

    // Finds the triangle around the start whose far side the segment leaves through.
    std::int32_t triangle = vertexTriangles[static_cast<std::size_t>(from)];
    std::int32_t right = -1;
    std::int32_t left = -1;
    for (std::size_t visited = 0; visited <= triangles.size(); ++visited) {
        const auto corner = static_cast<std::size_t>(cornerIndex(triangle, from));
        const Triangle& current = triangles[static_cast<std::size_t>(triangle)];
        const std::int32_t u = current.vertices[(corner + 1) % 3];
        const std::int32_t w = current.vertices[(corner + 2) % 3];
        if (between(u) || between(w)) {
            onSegment = between(u) ? u : w;
            return false;
        }
        if (orient(a, pointAt(u), b) > 0 && orient(a, pointAt(w), b) < 0) {
            right = u;
            left = w;
            break;
        }
        triangle = current.neighbors[(corner + 2) % 3];
    }
    if (right < 0) {
        throw std::runtime_error("The navigation mesh triangulation lost a constraint.");
    }

    // Walks from triangle to triangle across the edges the segment crosses, keeping the vertex on its right first.
    for (;;) {
        crossed.emplace_back(right, left);
        const std::int32_t next = triangles[static_cast<std::size_t>(triangle)].neighbors[static_cast<std::size_t>(edgeIndex(triangle, right, left))];
        const Triangle& beyond = triangles[static_cast<std::size_t>(next)];
        const std::int32_t apex = beyond.vertices[(static_cast<std::size_t>(edgeIndex(next, left, right)) + 2) % 3];
        if (apex == to) {
            return true;
        }
        if (between(apex)) {
            onSegment = apex;
            return false;
        }
        if (orient(a, b, pointAt(apex)) < 0) {
            right = apex;
        } else {
            left = apex;
        }
        triangle = next;
    }
}

void ConstrainedTriangulation::insertConstraint(std::int32_t from, std::int32_t to) {
    if (findEdge(from, to) >= 0 || findEdge(to, from) >= 0) {
        setConstrained(from, to);
        return;
    }

    // A vertex lying exactly on the segment splits it into two constraints.
    std::deque<Edge> crossed;
    std::int32_t onSegment = -1;
    if (!collectCrossings(from, to, crossed, onSegment)) {
        insertConstraint(from, onSegment);
        insertConstraint(onSegment, to);
        return;
    }

    // Flips every crossed edge whose quad is convex until none crosses the segment, as in the algorithm of Sloan.
    std::vector<Edge> created;
    const std::size_t limit = 64 * (crossed.size() + 8);
    for (std::size_t round = 0; !crossed.empty(); ++round) {
        if (round > limit) {
            throw std::runtime_error("The navigation mesh triangulation did not converge.");
        }
        const Edge edge = crossed.front();
        crossed.pop_front();
        const std::int32_t triangle = findEdge(edge.first, edge.second);
        const int side = edgeIndex(triangle, edge.first, edge.second);
        const std::int32_t apex = triangles[static_cast<std::size_t>(triangle)].vertices[static_cast<std::size_t>((side + 2) % 3)];
        const std::int32_t other = triangles[static_cast<std::size_t>(triangle)].neighbors[static_cast<std::size_t>(side)];
        const std::int32_t facing = triangles[static_cast<std::size_t>(other)].vertices[(static_cast<std::size_t>(edgeIndex(other, edge.second, edge.first)) + 2) % 3];
        if (!crosses(apex, facing, edge.first, edge.second)) {
            crossed.push_back(edge);
            continue;
        }

        flip(triangle, side);
        if (crosses(from, to, apex, facing)) {
            crossed.emplace_back(apex, facing);
        } else {
            created.emplace_back(apex, facing);
        }
    }
    setConstrained(from, to);

    // The new edges may break the Delaunay rule, which flips restore without touching constraints.
    for (bool changed = true; changed;) {
        changed = false;
        for (Edge& edge : created) {
            const std::int32_t triangle = findEdge(edge.first, edge.second);
            if (triangle < 0) {
                continue;
            }
            const int side = edgeIndex(triangle, edge.first, edge.second);
            const Triangle& current = triangles[static_cast<std::size_t>(triangle)];
            if (current.constrained[static_cast<std::size_t>(side)] || current.neighbors[static_cast<std::size_t>(side)] < 0) {
                continue;
            }
            const std::int32_t apex = current.vertices[static_cast<std::size_t>((side + 2) % 3)];
            const std::int32_t other = current.neighbors[static_cast<std::size_t>(side)];
            const std::int32_t facing = triangles[static_cast<std::size_t>(other)].vertices[(static_cast<std::size_t>(edgeIndex(other, edge.second, edge.first)) + 2) % 3];
            if (inCircle(pointAt(edge.first), pointAt(edge.second), pointAt(apex), pointAt(facing))) {
                flip(triangle, side);
                edge = {apex, facing};
                changed = true;
            }
        }
    }
}

double ConstrainedTriangulation::distance(const Point& a, const Point& b) noexcept {
    return std::hypot(b.x - a.x, b.y - a.y);
}

std::optional<ConstrainedTriangulation::Point> ConstrainedTriangulation::projectInside(const Point& point, const Point& a, const Point& b) noexcept {
    const Point direction{b.x - a.x, b.y - a.y};
    const double along = ((point.x - a.x) * direction.x + (point.y - a.y) * direction.y) / (direction.x * direction.x + direction.y * direction.y);
    if (!(along > 0.0 && along < 1.0)) {
        return std::nullopt;
    }
    return Point{a.x + direction.x * along, a.y + direction.y * along};
}

std::optional<ConstrainedTriangulation::Side> ConstrainedTriangulation::findConstraint(const Point& point, Side side, double reach) const {
    // Every side beyond lies farther from the point than the one before, which ends the walk.
    double nearest = 0.0;
    for (;;) {
        const Triangle& current = triangles[static_cast<std::size_t>(side.triangle)];
        const auto index = static_cast<std::size_t>(side.index);
        const std::int32_t a = current.vertices[index];
        const std::int32_t b = current.vertices[(index + 1) % 3];
        const std::optional<Point> foot = projectInside(point, pointAt(a), pointAt(b));
        const double gap = foot ? distance(point, *foot) : reach;
        if (gap >= reach || gap <= nearest) {
            return std::nullopt;
        }
        if (current.constrained[index]) {
            return side;
        }
        const std::int32_t next = current.neighbors[index];
        if (next < 0) {
            return std::nullopt;
        }

        // By the Delaunay rule only the longer of the two far sides of the next triangle can hold a point nearer than the reach.
        const int back = edgeIndex(next, b, a);
        const std::int32_t apex = triangles[static_cast<std::size_t>(next)].vertices[static_cast<std::size_t>((back + 2) % 3)];
        side = {.triangle = next, .index = distance(pointAt(apex), pointAt(a)) > distance(pointAt(apex), pointAt(b)) ? (back + 1) % 3 : (back + 2) % 3};
        nearest = gap;
    }
}

bool ConstrainedTriangulation::refineCorner(std::int32_t triangle, int corner) {
    const Triangle& current = triangles[static_cast<std::size_t>(triangle)];
    const auto index = static_cast<std::size_t>(corner);
    if (current.constrained[index] || current.constrained[(index + 2) % 3]) {
        return false;
    }
    const Point apex = pointAt(current.vertices[index]);
    const Point& next = pointAt(current.vertices[(index + 1) % 3]);
    const Point& previous = pointAt(current.vertices[(index + 2) % 3]);
    const double reach = std::min(distance(apex, next), distance(apex, previous)) * (1.0 - kRoomSlack);
    const Side far{.triangle = triangle, .index = (corner + 1) % 3};

    // A vertex that comes too close lies between the corner and its mirror across the perpendicular bisector of the far side, which lies on the circumcircle, so testing both ends covers it.
    std::optional<Side> constraint = findConstraint(apex, far, reach);
    if (!constraint) {
        const Point direction{previous.x - next.x, previous.y - next.y};
        const double shift = 2.0 * ((apex.x - (next.x + previous.x) * 0.5) * direction.x + (apex.y - (next.y + previous.y) * 0.5) * direction.y) / (direction.x * direction.x + direction.y * direction.y);
        constraint = findConstraint({apex.x - direction.x * shift, apex.y - direction.y * shift}, far, reach);
    }
    if (!constraint) {
        return false;
    }

    // A foot that falls within the weld tolerance of an end of the edge would only duplicate that end.
    const Triangle& holder = triangles[static_cast<std::size_t>(constraint->triangle)];
    const Point& start = pointAt(holder.vertices[static_cast<std::size_t>(constraint->index)]);
    const Point& end = pointAt(holder.vertices[static_cast<std::size_t>((constraint->index + 1) % 3)]);
    const std::optional<Point> foot = projectInside(apex, start, end);
    if (!foot || distance(*foot, start) <= tolerance || distance(*foot, end) <= tolerance) {
        return false;
    }
    points.push_back(*foot);
    vertexTriangles.push_back(constraint->triangle);
    std::vector<std::pair<std::int32_t, int>> pending;
    splitEdge(constraint->triangle, constraint->index, static_cast<std::int32_t>(points.size() - 1), pending);
    legalize(pending);
    return true;
}

void ConstrainedTriangulation::refine() {
    for (bool changed = true; changed;) {
        changed = false;
        for (std::int32_t triangle = 0; triangle < static_cast<std::int32_t>(triangles.size()); ++triangle) {
            if (!kept[static_cast<std::size_t>(triangle)]) {
                continue;
            }
            for (int corner = 0; corner < 3; ++corner) {
                changed = refineCorner(triangle, corner) || changed;
            }
        }
    }
}

void ConstrainedTriangulation::classify(const std::function<bool(math::Vec2)>& keeps) {
    std::vector<bool> seen(triangles.size(), false);
    std::vector<std::int32_t> area;
    for (std::size_t seed = 0; seed < triangles.size(); ++seed) {
        if (seen[seed]) {
            continue;
        }

        // An area is every triangle reached from the seed without crossing a segment, and the area around everything touches the super triangle.
        area.assign(1, static_cast<std::int32_t>(seed));
        seen[seed] = true;
        bool enclosed = true;
        std::int32_t largest = area.front();
        double largestSize = 0.0;
        for (std::size_t next = 0; next < area.size(); ++next) {
            const Triangle& current = triangles[static_cast<std::size_t>(area[next])];
            enclosed = enclosed && std::ranges::none_of(current.vertices, [this](std::int32_t vertex) { return isSuper(vertex); });
            const Point& a = pointAt(current.vertices[0]);
            const Point& b = pointAt(current.vertices[1]);
            const Point& c = pointAt(current.vertices[2]);
            const double size = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            if (size > largestSize) {
                largest = area[next];
                largestSize = size;
            }
            for (std::size_t side = 0; side < 3; ++side) {
                const std::int32_t neighbor = current.neighbors[side];
                if (neighbor >= 0 && !current.constrained[side] && !seen[static_cast<std::size_t>(neighbor)]) {
                    seen[static_cast<std::size_t>(neighbor)] = true;
                    area.push_back(neighbor);
                }
            }
        }

        // The centroid of the largest triangle stands for the whole area, since no segment crosses it.
        const std::array<std::int32_t, 3>& corners = triangles[static_cast<std::size_t>(largest)].vertices;
        const math::Vec2 centroid{static_cast<float>((pointAt(corners[0]).x + pointAt(corners[1]).x + pointAt(corners[2]).x) / 3.0), static_cast<float>((pointAt(corners[0]).y + pointAt(corners[1]).y + pointAt(corners[2]).y) / 3.0)};
        const bool keep = enclosed && keeps(centroid);
        for (const std::int32_t triangle : area) {
            kept[static_cast<std::size_t>(triangle)] = keep;
        }
    }
}

void ConstrainedTriangulation::finish() {
    vertices.clear();
    for (std::int32_t vertex = 0; vertex < static_cast<std::int32_t>(points.size()); ++vertex) {
        if (!isSuper(vertex)) {
            vertices.push_back({static_cast<float>(pointAt(vertex).x), static_cast<float>(pointAt(vertex).y)});
        }
    }

    // Only the kept triangles stay, their neighbors are renumbered, and the points added after the super triangle move down to fill its three corners.
    std::vector<std::int32_t> remap(triangles.size(), -1);
    std::vector<Triangle> output;
    for (std::size_t triangle = 0; triangle < triangles.size(); ++triangle) {
        if (kept[triangle]) {
            remap[triangle] = static_cast<std::int32_t>(output.size());
            output.push_back(triangles[triangle]);
        }
    }
    for (Triangle& triangle : output) {
        for (std::int32_t& neighbor : triangle.neighbors) {
            neighbor = neighbor < 0 ? -1 : remap[static_cast<std::size_t>(neighbor)];
        }
        for (std::int32_t& vertex : triangle.vertices) {
            vertex = vertex < firstSuper ? vertex : vertex - 3;
        }
    }
    triangles = std::move(output);
}

void ConstrainedTriangulation::build(std::span<const math::Segment> segments, const std::function<bool(math::Vec2)>& keeps) {
    triangles.clear();
    vertices.clear();
    prepare(segments);
    if (constraints.empty()) {
        return;
    }

    // Inserting the points from left to right keeps each walk to the next point short.
    std::vector<std::int32_t> order(points.size());
    std::iota(order.begin(), order.end(), 0);
    std::ranges::sort(order, [this](std::int32_t lhs, std::int32_t rhs) { return pointAt(lhs).x != pointAt(rhs).x ? pointAt(lhs).x < pointAt(rhs).x : pointAt(lhs).y < pointAt(rhs).y; });
    addSuperTriangle();
    for (const std::int32_t vertex : order) {
        insertPoint(vertex);
    }
    for (const auto& [from, to] : constraints) {
        insertConstraint(from, to);
    }
    classify(keeps);
    refine();
    finish();
}

} // namespace haylen::navigation2d
