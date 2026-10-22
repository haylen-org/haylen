#include "math/Triangulator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "haylen/math/Geometry.hpp"

namespace haylen::math {

float Triangulator::turn(Vec2 a, Vec2 b, Vec2 c) noexcept {
    return Vec2::cross(b - a, c - b);
}

// A point on an edge or a corner of the triangle counts as inside it, whichever way the triangle turns.
bool Triangulator::inTriangle(Vec2 a, Vec2 b, Vec2 c, Vec2 point) noexcept {
    const float first = Vec2::cross(b - a, point - a);
    const float second = Vec2::cross(c - b, point - b);
    const float third = Vec2::cross(a - c, point - c);
    const bool negative = first < 0.0F || second < 0.0F || third < 0.0F;
    const bool positive = first > 0.0F || second > 0.0F || third > 0.0F;
    return !(negative && positive);
}

bool Triangulator::inBox(Vec2 a, Vec2 point, Vec2 b) noexcept {
    return point.x <= std::max(a.x, b.x) && point.x >= std::min(a.x, b.x) && point.y <= std::max(a.y, b.y) && point.y >= std::min(a.y, b.y);
}

// Two segments cross when each one has the ends of the other on both of its sides, or when an end of one lies on the other.
bool Triangulator::crosses(Vec2 firstStart, Vec2 firstEnd, Vec2 secondStart, Vec2 secondEnd) noexcept {
    const auto side = [](float value) { return (value > 0.0F) - (value < 0.0F); };
    const int first = side(turn(firstStart, firstEnd, secondStart));
    const int second = side(turn(firstStart, firstEnd, secondEnd));
    const int third = side(turn(secondStart, secondEnd, firstStart));
    const int fourth = side(turn(secondStart, secondEnd, firstEnd));
    if (first != second && third != fourth) {
        return true;
    }
    return (first == 0 && inBox(firstStart, secondStart, firstEnd)) || (second == 0 && inBox(firstStart, secondEnd, firstEnd)) || (third == 0 && inBox(secondStart, firstStart, secondEnd)) || (fourth == 0 && inBox(secondStart, firstEnd, secondEnd));
}

void Triangulator::triangulate(std::span<const Vec2> shapePoints, std::span<const std::uint32_t> counts) {
    points = shapePoints;
    triangles.clear();
    classify(counts);

    // Each filled region clips as one ring, its outer outline joined to its holes from the leftmost hole to the right.
    for (std::uint32_t outer = 0; outer < outlines.size(); ++outer) {
        if (kinds[outer] != Kind::Outer) {
            continue;
        }
        nodes.clear();
        std::uint32_t ring = link(outlines[outer], true);
        holes.clear();
        for (std::uint32_t hole = 0; hole < outlines.size(); ++hole) {
            if (kinds[hole] == Kind::Hole && outlines[hole].region == outer) {
                holes.push_back(getLeftmost(link(outlines[hole], false)));
            }
        }
        // clang-format off
        std::sort(holes.begin(), holes.end(), [this](std::uint32_t lhs, std::uint32_t rhs) {
            const Vec2 left = at(lhs);
            const Vec2 right = at(rhs);
            return left.x < right.x || (left.x == right.x && left.y < right.y);
        });
        // clang-format on
        for (const std::uint32_t hole : holes) {
            ring = joinHole(hole, ring);
        }
        clip(ring);
    }
}

// The outlines meet the outlines around them in order of falling area, so the parent of an outline, the smallest outline that encloses it, has its winding before it.
void Triangulator::classify(std::span<const std::uint32_t> counts) {
    outlines.clear();
    order.clear();
    std::uint32_t first = 0;
    for (const std::uint32_t count : counts) {
        const std::span<const Vec2> run = points.subspan(first, count);
        Outline outline{.first = first, .count = count, .low = run.empty() ? Vec2{} : run.front(), .high = run.empty() ? Vec2{} : run.front(), .area = count >= 3 ? Geometry::signedArea(run) : 0.0F};
        for (const Vec2 point : run) {
            outline.low = Vec2::min(outline.low, point);
            outline.high = Vec2::max(outline.high, point);
        }
        outlines.push_back(outline);
        order.push_back(static_cast<std::uint32_t>(order.size()));
        first += count;
    }
    // clang-format off
    std::sort(order.begin(), order.end(), [this](std::uint32_t lhs, std::uint32_t rhs) {
        return std::fabs(outlines[lhs].area) > std::fabs(outlines[rhs].area);
    });
    // clang-format on

    kinds.assign(outlines.size(), Kind::Inside);
    for (std::size_t rank = 0; rank < order.size(); ++rank) {
        Outline& outline = outlines[order[rank]];
        float parentArea = std::numeric_limits<float>::infinity();
        for (std::size_t larger = 0; larger < rank; ++larger) {
            const Outline& candidate = outlines[order[larger]];
            if (std::fabs(candidate.area) < parentArea && encloses(candidate, outline)) {
                outline.parent = order[larger];
                parentArea = std::fabs(candidate.area);
            }
        }

        // The fill winds around the outline as much as inside its parent, and the outline adds its own turn, so it bounds the fill where one side winds and the other does not.
        const int around = outline.parent == kNone ? 0 : outlines[outline.parent].winding;
        const int sign = (outline.area > 0.0F) - (outline.area < 0.0F);
        outline.winding = around + sign;
        Kind& kind = kinds[order[rank]];
        if (sign != 0 && around == 0) {
            kind = Kind::Outer;
        } else if (sign != 0 && outline.winding == 0) {
            kind = Kind::Hole;
            outline.region = outline.parent;
            while (kinds[outline.region] != Kind::Outer) {
                outline.region = outlines[outline.region].parent;
            }
        }
    }
}

// An outline encloses another when it holds the bounds of the other and the first point of the other off its edge lies inside it, so outlines that only share edges, such as neighbouring pieces, enclose nothing.
bool Triangulator::encloses(const Outline& outer, const Outline& inner) const noexcept {
    const bool within = inner.low.x >= outer.low.x && inner.low.y >= outer.low.y && inner.high.x <= outer.high.x && inner.high.y <= outer.high.y;
    if (outer.area == 0.0F || !within) {
        return false;
    }
    for (const Vec2 point : points.subspan(inner.first, inner.count)) {
        const Side side = locate(outer, point);
        if (side != Side::Edge) {
            return side == Side::Inside;
        }
    }
    return false;
}

Triangulator::Side Triangulator::locate(const Outline& outline, Vec2 point) const noexcept {
    const std::span<const Vec2> run = points.subspan(outline.first, outline.count);
    bool inside = false;
    for (std::size_t current = 0, previous = run.size() - 1; current < run.size(); previous = current++) {
        const Vec2 a = run[previous];
        const Vec2 b = run[current];
        const Vec2 edge = b - a;
        const float length = edge.getLength();
        const float tolerance = kEdgeTolerance * (1.0F + std::max({std::fabs(a.x), std::fabs(a.y), std::fabs(b.x), std::fabs(b.y)}));
        const float along = Vec2::dot(point - a, edge);
        if (std::fabs(Vec2::cross(edge, point - a)) <= tolerance * length && along >= -tolerance * length && along <= length * (length + tolerance)) {
            return Side::Edge;
        }
        if ((a.y > point.y) != (b.y > point.y) && point.x < edge.x * (point.y - a.y) / edge.y + a.x) {
            inside = !inside;
        }
    }
    return inside ? Side::Inside : Side::Outside;
}

// Links the points of an outline into a ring that turns the positive way, or the negative way for a hole.
std::uint32_t Triangulator::link(const Outline& outline, bool positive) {
    const bool forward = (outline.area > 0.0F) == positive;
    const auto first = static_cast<std::uint32_t>(nodes.size());
    for (std::uint32_t step = 0; step < outline.count; ++step) {
        const std::uint32_t index = static_cast<std::uint32_t>(nodes.size());
        nodes.push_back({.point = outline.first + (forward ? step : outline.count - 1 - step), .previous = index - 1, .next = index + 1});
    }
    const auto last = static_cast<std::uint32_t>(nodes.size() - 1);
    nodes[first].previous = last;
    nodes[last].next = first;
    return first;
}

std::uint32_t Triangulator::getLeftmost(std::uint32_t start) const noexcept {
    std::uint32_t node = start;
    std::uint32_t leftmost = start;
    do {
        const Vec2 point = at(node);
        const Vec2 best = at(leftmost);
        if (point.x < best.x || (point.x == best.x && point.y < best.y)) {
            leftmost = node;
        }
        node = nodes[node].next;
    } while (node != start);
    return leftmost;
}

// Cuts the ring open at the bridge and runs it around the hole and back, then drops the points the cut leaves on one line.
std::uint32_t Triangulator::joinHole(std::uint32_t hole, std::uint32_t ring) {
    const std::uint32_t bridge = findBridge(hole, ring);
    if (bridge == kNone) {
        return ring;
    }
    const std::uint32_t back = split(bridge, hole);
    static_cast<void>(filter(back, nodes[back].next));
    return filter(bridge, nodes[bridge].next);
}

// A ray from the leftmost point of the hole to the left meets the nearest edge of the ring that faces it, and the end of that edge farther left is the bridge, unless a reflex vertex of the ring inside the triangle of the hole point, the meeting point and that end stands in the way, in which case the one closest in angle to the ray is.
std::uint32_t Triangulator::findBridge(std::uint32_t hole, std::uint32_t ring) const noexcept {
    const Vec2 from = at(hole);
    std::uint32_t node = ring;
    std::uint32_t bridge = kNone;
    float reach = -std::numeric_limits<float>::infinity();
    if (from == at(node)) {
        return node;
    }
    do {
        const std::uint32_t next = nodes[node].next;
        const Vec2 start = at(node);
        const Vec2 end = at(next);
        if (from == end) {
            return next;
        }
        if (from.y <= start.y && from.y >= end.y && end.y != start.y) {
            const float x = start.x + (from.y - start.y) * (end.x - start.x) / (end.y - start.y);
            if (x <= from.x && x > reach) {
                reach = x;
                bridge = start.x < end.x ? node : next;
                if (x == from.x) {
                    return bridge;
                }
            }
        }
        node = next;
    } while (node != ring);
    if (bridge == kNone) {
        return kNone;
    }

    const std::uint32_t stop = bridge;
    const Vec2 corner = at(bridge);
    const Vec2 meeting{reach, from.y};
    float least = std::numeric_limits<float>::infinity();
    node = bridge;
    do {
        const Vec2 point = at(node);
        if (from.x >= point.x && point.x >= corner.x && from.x != point.x && inTriangle(from.y < corner.y ? from : meeting, corner, from.y < corner.y ? meeting : from, point)) {
            const float tangent = std::fabs(from.y - point.y) / (from.x - point.x);
            const Vec2 chosen = at(bridge);
            if (isLocallyInside(node, hole) && (tangent < least || (tangent == least && (point.x > chosen.x || (point.x == chosen.x && sectorContains(bridge, node)))))) {
                bridge = node;
                least = tangent;
            }
        }
        node = nodes[node].next;
    } while (node != stop);
    return bridge;
}

// Joins two vertices of the ring, or of the ring and a hole, by two edges that run both ways between them, and returns the copy of the second vertex that ends the way back.
std::uint32_t Triangulator::split(std::uint32_t from, std::uint32_t to) {
    const auto fromCopy = static_cast<std::uint32_t>(nodes.size());
    nodes.push_back({.point = nodes[from].point});
    const auto toCopy = static_cast<std::uint32_t>(nodes.size());
    nodes.push_back({.point = nodes[to].point});
    const std::uint32_t afterFrom = nodes[from].next;
    const std::uint32_t beforeTo = nodes[to].previous;

    nodes[from].next = to;
    nodes[to].previous = from;
    nodes[fromCopy].next = afterFrom;
    nodes[afterFrom].previous = fromCopy;
    nodes[toCopy].next = fromCopy;
    nodes[fromCopy].previous = toCopy;
    nodes[beforeTo].next = toCopy;
    nodes[toCopy].previous = beforeTo;
    return toCopy;
}

// Drops the vertices that repeat their next one or lie on the line of their neighbours, from `start` until `end`, and returns a vertex that stays.
std::uint32_t Triangulator::filter(std::uint32_t start, std::uint32_t end) noexcept {
    std::uint32_t node = start;
    bool again = false;
    do {
        again = false;
        const std::uint32_t next = nodes[node].next;
        if (at(node) == at(next) || turnAt(nodes[node].previous, node, next) == 0.0F) {
            remove(node);
            node = end = nodes[node].previous;
            if (node == nodes[node].next) {
                break;
            }
            again = true;
        } else {
            node = next;
        }
    } while (again || node != end);
    return end;
}

// Clips the triangle of every two edges of the ring that cross right after each other, which undoes the small loops a ring that touches itself leaves.
std::uint32_t Triangulator::cureCrossings(std::uint32_t start) {
    std::uint32_t node = start;
    do {
        const std::uint32_t before = nodes[node].previous;
        const std::uint32_t next = nodes[node].next;
        const std::uint32_t after = nodes[next].next;
        if (at(before) != at(after) && crosses(at(before), at(node), at(next), at(after)) && isLocallyInside(before, after) && isLocallyInside(after, before)) {
            emit(before, node, after);
            remove(node);
            remove(next);
            node = start = after;
        }
        node = nodes[node].next;
    } while (node != start);
    return filter(node, node);
}

// Clips ears until the ring runs out, and when a whole round finds none, drops the vertices on one line and then undoes the small crossings before it tries again, giving up on a ring that still has no ear.
void Triangulator::clip(std::uint32_t ear) {
    for (int pass = 0; pass < 3; ++pass) {
        if (pass == 1) {
            ear = filter(ear, ear);
        } else if (pass == 2) {
            ear = cureCrossings(filter(ear, ear));
        }

        std::uint32_t stop = ear;
        bool stuck = false;
        while (nodes[ear].previous != nodes[ear].next) {
            const std::uint32_t previous = nodes[ear].previous;
            const std::uint32_t next = nodes[ear].next;
            if (isEar(ear)) {
                emit(previous, ear, next);
                remove(ear);
                ear = nodes[next].next;
                stop = ear;
                continue;
            }
            ear = next;
            if (ear == stop) {
                stuck = true;
                break;
            }
        }
        if (!stuck) {
            return;
        }
    }
}

// A convex vertex is an ear when no reflex vertex of the ring other than its own corners lies in its triangle.
bool Triangulator::isEar(std::uint32_t ear) const noexcept {
    const std::uint32_t previous = nodes[ear].previous;
    const std::uint32_t next = nodes[ear].next;
    const Vec2 a = at(previous);
    const Vec2 b = at(ear);
    const Vec2 c = at(next);
    if (turn(a, b, c) <= 0.0F) {
        return false;
    }

    const Vec2 low = Vec2::min(a, Vec2::min(b, c));
    const Vec2 high = Vec2::max(a, Vec2::max(b, c));
    for (std::uint32_t node = nodes[next].next; node != previous; node = nodes[node].next) {
        const Vec2 point = at(node);
        if (point.x >= low.x && point.x <= high.x && point.y >= low.y && point.y <= high.y && point != a && inTriangle(a, b, c, point) && turnAt(nodes[node].previous, node, nodes[node].next) <= 0.0F) {
            return false;
        }
    }
    return true;
}

// Tells whether the diagonal from one vertex toward another leaves the first one into the inside of the ring.
bool Triangulator::isLocallyInside(std::uint32_t from, std::uint32_t to) const noexcept {
    const std::uint32_t previous = nodes[from].previous;
    const std::uint32_t next = nodes[from].next;
    if (turnAt(previous, from, next) > 0.0F) {
        return turnAt(from, to, next) <= 0.0F && turnAt(from, previous, to) <= 0.0F;
    }
    return turnAt(from, to, previous) > 0.0F || turnAt(from, next, to) > 0.0F;
}

// Tells whether the corner of the ring at `inner` lies within the corner at `outer`, which picks between two bridges that stand on the same point.
bool Triangulator::sectorContains(std::uint32_t outer, std::uint32_t inner) const noexcept {
    return turnAt(nodes[outer].previous, outer, nodes[inner].previous) > 0.0F && turnAt(nodes[inner].next, outer, nodes[outer].next) > 0.0F;
}

void Triangulator::remove(std::uint32_t node) noexcept {
    const Node& removed = nodes[node];
    nodes[removed.previous].next = removed.next;
    nodes[removed.next].previous = removed.previous;
}

void Triangulator::emit(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    triangles.insert(triangles.end(), {nodes[a].point, nodes[b].point, nodes[c].point});
}

} // namespace haylen::math
