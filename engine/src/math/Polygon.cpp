#include "haylen/math/Polygon.hpp"

#include <algorithm>
#include <array>
#include <clipper2/clipper.h>
#include <clipper2/clipper.triangulation.h>
#include <cmath>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "haylen/math/Geometry.hpp"

namespace haylen::math {

const std::array<std::pair<std::string_view, Polygon::Join>, 4> Polygon::kJoinNames{{{"miter", Join::Miter}, {"round", Join::Round}, {"square", Join::Square}, {"bevel", Join::Bevel}}};

std::optional<Polygon::Join> Polygon::joinFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kJoinNames, name, &std::pair<std::string_view, Join>::first);
    return found != kJoinNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Polygon::joinName(Join value) noexcept {
    return std::ranges::find(kJoinNames, value, &std::pair<std::string_view, Join>::second)->first;
}

// Converts shapes to the paths of Clipper2 and back.
class Polygon::Clipper final {
  public:
    [[nodiscard]] static Clipper2Lib::PathsD toPaths(std::span<const Outline> shape) {
        Clipper2Lib::PathsD paths;
        paths.reserve(shape.size());
        for (const Outline& outline : shape) {
            Clipper2Lib::PathD& path = paths.emplace_back();
            path.reserve(outline.size());
            for (const Vec2 point : outline) {
                path.emplace_back(static_cast<double>(point.x), static_cast<double>(point.y));
            }
        }
        return paths;
    }

    [[nodiscard]] static std::vector<Outline> toShape(const Clipper2Lib::PathsD& paths) {
        std::vector<Outline> shape;
        shape.reserve(paths.size());
        for (const Clipper2Lib::PathD& path : paths) {
            Outline& outline = shape.emplace_back();
            outline.reserve(path.size());
            for (const Clipper2Lib::PointD& point : path) {
                outline.push_back({static_cast<float>(point.x), static_cast<float>(point.y)});
            }
        }
        return shape;
    }

    [[nodiscard]] static Clipper2Lib::Paths64 toIntegerPaths(std::span<const Outline> shape) {
        Clipper2Lib::Paths64 paths;
        paths.reserve(shape.size());
        for (const Outline& outline : shape) {
            Clipper2Lib::Path64& path = paths.emplace_back();
            path.reserve(outline.size());
            for (const Vec2 point : outline) {
                path.emplace_back(std::llround(static_cast<double>(point.x) * kScale), std::llround(static_cast<double>(point.y) * kScale));
            }
        }
        return paths;
    }

    [[nodiscard]] static Clipper2Lib::JoinType toJoinType(Join join) noexcept {
        switch (join) {
        case Join::Miter:
            return Clipper2Lib::JoinType::Miter;
        case Join::Square:
            return Clipper2Lib::JoinType::Square;
        case Join::Bevel:
            return Clipper2Lib::JoinType::Bevel;
        case Join::Round:
            break;
        }
        return Clipper2Lib::JoinType::Round;
    }
};

// Merges the triangles of a triangulation into convex pieces with the Hertel-Mehlhorn heuristic, trying the longest shared edges first.
class Polygon::Merger final {
  public:
    explicit Merger(const Clipper2Lib::Paths64& triangles) {
        for (const Clipper2Lib::Path64& triangle : triangles) {
            if (triangle.size() != 3) {
                continue;
            }
            std::vector<std::uint32_t> piece{vertexOf(triangle[0]), vertexOf(triangle[1]), vertexOf(triangle[2])};
            const double turn = cross(piece[0], piece[1], piece[2]);
            if (turn == 0.0) {
                continue;
            }
            if (turn < 0.0) {
                std::swap(piece[1], piece[2]);
            }
            addPiece(std::move(piece));
        }
    }

    void merge(std::size_t maxVertices) {
        struct SharedEdge {
            std::uint32_t from = 0;
            std::uint32_t to = 0;
            double lengthSquared = 0.0;
        };

        std::vector<SharedEdge> shared;
        for (const auto& [key, owner] : owners) {
            const auto from = static_cast<std::uint32_t>(key >> 32U);
            const auto to = static_cast<std::uint32_t>(key & 0xFFFFFFFFU);
            if (from < to && owners.contains(edgeKey(to, from))) {
                const double dx = static_cast<double>(vertices[to].x - vertices[from].x);
                const double dy = static_cast<double>(vertices[to].y - vertices[from].y);
                shared.push_back({from, to, dx * dx + dy * dy});
            }
        }
        // clang-format off
        std::sort(shared.begin(), shared.end(), [](const SharedEdge& lhs, const SharedEdge& rhs) {
            return lhs.lengthSquared > rhs.lengthSquared || (lhs.lengthSquared == rhs.lengthSquared && std::pair(lhs.from, lhs.to) < std::pair(rhs.from, rhs.to));
        });
        // clang-format on

        for (const SharedEdge& edge : shared) {
            const auto first = owners.find(edgeKey(edge.from, edge.to));
            const auto second = owners.find(edgeKey(edge.to, edge.from));
            if (first == owners.end() || second == owners.end() || first->second == second->second) {
                continue;
            }
            tryMerge(first->second, second->second, edge.from, edge.to, maxVertices);
        }
    }

    [[nodiscard]] std::vector<Outline> getPieces() const {
        std::vector<Outline> result;
        for (std::size_t index = 0; index < pieces.size(); ++index) {
            if (!alive[index]) {
                continue;
            }
            const std::vector<std::uint32_t> corners = withoutCollinear(pieces[index]);
            Outline& outline = result.emplace_back();
            outline.reserve(corners.size());
            for (const std::uint32_t vertex : corners) {
                outline.push_back({static_cast<float>(static_cast<double>(vertices[vertex].x) / kScale), static_cast<float>(static_cast<double>(vertices[vertex].y) / kScale)});
            }
        }
        return result;
    }

  private:
    [[nodiscard]] static std::uint64_t edgeKey(std::uint32_t from, std::uint32_t to) noexcept {
        return (static_cast<std::uint64_t>(from) << 32U) | to;
    }

    [[nodiscard]] std::uint32_t vertexOf(const Clipper2Lib::Point64& point) {
        const auto [found, inserted] = ids.try_emplace({point.x, point.y}, static_cast<std::uint32_t>(vertices.size()));
        if (inserted) {
            vertices.push_back(point);
        }
        return found->second;
    }

    [[nodiscard]] double cross(std::uint32_t a, std::uint32_t b, std::uint32_t c) const noexcept {
        const Clipper2Lib::Point64& first = vertices[a];
        const Clipper2Lib::Point64& middle = vertices[b];
        const Clipper2Lib::Point64& last = vertices[c];
        return static_cast<double>(middle.x - first.x) * static_cast<double>(last.y - middle.y) - static_cast<double>(middle.y - first.y) * static_cast<double>(last.x - middle.x);
    }

    void addPiece(std::vector<std::uint32_t> piece) {
        const auto index = static_cast<std::uint32_t>(pieces.size());
        for (std::size_t corner = 0; corner < piece.size(); ++corner) {
            owners[edgeKey(piece[corner], piece[(corner + 1) % piece.size()])] = index;
        }
        pieces.push_back(std::move(piece));
        alive.push_back(true);
    }

    [[nodiscard]] std::vector<std::uint32_t> withoutCollinear(const std::vector<std::uint32_t>& piece) const {
        std::vector<std::uint32_t> corners;
        corners.reserve(piece.size());
        for (std::size_t corner = 0; corner < piece.size(); ++corner) {
            const std::uint32_t previous = piece[(corner + piece.size() - 1) % piece.size()];
            const std::uint32_t next = piece[(corner + 1) % piece.size()];
            if (cross(previous, piece[corner], next) != 0.0) {
                corners.push_back(piece[corner]);
            }
        }
        return corners;
    }

    // Starts a copy of the piece at the given vertex, so the edge that ends there is the last one.
    [[nodiscard]] static std::vector<std::uint32_t> rotatedTo(const std::vector<std::uint32_t>& piece, std::uint32_t start) {
        std::vector<std::uint32_t> rotated(piece);
        std::rotate(rotated.begin(), std::find(rotated.begin(), rotated.end(), start), rotated.end());
        return rotated;
    }

    // Joins the piece that owns `from->to` with the piece that owns `to->from` when the union stays convex and small enough.
    void tryMerge(std::uint32_t firstIndex, std::uint32_t secondIndex, std::uint32_t from, std::uint32_t to, std::size_t maxVertices) {
        const std::vector<std::uint32_t> first = rotatedTo(pieces[firstIndex], to);
        const std::vector<std::uint32_t> second = rotatedTo(pieces[secondIndex], from);
        if (cross(first[first.size() - 2], from, second[1]) < 0.0 || cross(second[second.size() - 2], to, first[1]) < 0.0) {
            return;
        }

        std::vector<std::uint32_t> merged(first);
        merged.insert(merged.end(), second.begin() + 1, second.end() - 1);
        if (withoutCollinear(merged).size() > maxVertices) {
            return;
        }

        owners.erase(edgeKey(from, to));
        owners.erase(edgeKey(to, from));
        for (std::size_t corner = 0; corner + 1 < second.size(); ++corner) {
            owners[edgeKey(second[corner], second[corner + 1])] = firstIndex;
        }
        pieces[firstIndex] = std::move(merged);
        alive[secondIndex] = false;
    }

    std::vector<Clipper2Lib::Point64> vertices;
    std::map<std::pair<std::int64_t, std::int64_t>, std::uint32_t> ids;
    std::vector<std::vector<std::uint32_t>> pieces;
    std::vector<bool> alive;
    std::unordered_map<std::uint64_t, std::uint32_t> owners;
};

std::vector<Polygon::Outline> Polygon::combine(Operation operation, std::span<const Outline> subjects, std::span<const Outline> clips) {
    Clipper2Lib::ClipType type = Clipper2Lib::ClipType::Union;
    switch (operation) {
    case Operation::Difference:
        type = Clipper2Lib::ClipType::Difference;
        break;
    case Operation::Intersection:
        type = Clipper2Lib::ClipType::Intersection;
        break;
    case Operation::Xor:
        type = Clipper2Lib::ClipType::Xor;
        break;
    case Operation::Union:
        break;
    }
    return Clipper::toShape(Clipper2Lib::BooleanOp(type, Clipper2Lib::FillRule::NonZero, Clipper::toPaths(subjects), Clipper::toPaths(clips), kPrecision));
}

std::vector<Polygon::Outline> Polygon::unite(std::span<const Outline> subjects, std::span<const Outline> clips) {
    return combine(Operation::Union, subjects, clips);
}

std::vector<Polygon::Outline> Polygon::subtract(std::span<const Outline> subjects, std::span<const Outline> clips) {
    return combine(Operation::Difference, subjects, clips);
}

std::vector<Polygon::Outline> Polygon::intersect(std::span<const Outline> subjects, std::span<const Outline> clips) {
    return combine(Operation::Intersection, subjects, clips);
}

std::vector<Polygon::Outline> Polygon::exclude(std::span<const Outline> subjects, std::span<const Outline> clips) {
    return combine(Operation::Xor, subjects, clips);
}

std::vector<Polygon::Outline> Polygon::offset(std::span<const Outline> shape, float distance, Join join, float miterLimit) {
    const double arcTolerance = std::fabs(static_cast<double>(distance)) * static_cast<double>(kArcTolerance);
    const Clipper2Lib::PathsD normalized = Clipper2Lib::Union(Clipper::toPaths(shape), Clipper2Lib::FillRule::NonZero, kPrecision);
    return Clipper::toShape(Clipper2Lib::InflatePaths(normalized, static_cast<double>(distance), Clipper::toJoinType(join), Clipper2Lib::EndType::Polygon, static_cast<double>(miterLimit), kPrecision, arcTolerance));
}

float Polygon::distanceSquaredToSegment(Vec2 point, Vec2 start, Vec2 end) noexcept {
    const Vec2 direction = end - start;
    const float lengthSquared = direction.getLengthSquared();
    if (lengthSquared <= 0.0F) {
        return Vec2::distanceSquared(point, start);
    }
    const float t = std::clamp(Vec2::dot(point - start, direction) / lengthSquared, 0.0F, 1.0F);
    return Vec2::distanceSquared(point, start + direction * t);
}

// Keeps the farthest point of every span that strays beyond the tolerance, with an explicit stack so long lines never exhaust the call stack.
void Polygon::simplifyRange(std::span<const Vec2> points, std::size_t first, std::size_t last, float toleranceSquared, std::vector<bool>& kept) {
    std::vector<std::pair<std::size_t, std::size_t>> spans{{first, last}};
    while (!spans.empty()) {
        const auto [start, end] = spans.back();
        spans.pop_back();

        float farthest = toleranceSquared;
        std::size_t split = start;
        for (std::size_t index = start + 1; index < end; ++index) {
            const float distance = distanceSquaredToSegment(points[index], points[start], points[end]);
            if (distance > farthest) {
                farthest = distance;
                split = index;
            }
        }

        if (split != start) {
            kept[split] = true;
            spans.emplace_back(start, split);
            spans.emplace_back(split, end);
        }
    }
}

Polygon::Outline Polygon::simplify(std::span<const Vec2> points, float tolerance, bool closed) {
    if (points.size() < 3) {
        return {points.begin(), points.end()};
    }

    // A closed outline is split at the point farthest from its first point, so both halves are open lines with fixed ends.
    std::vector<Vec2> line(points.begin(), points.end());
    std::size_t middle = line.size() - 1;
    if (closed) {
        line.push_back(points.front());
        const auto farthest = std::max_element(points.begin(), points.end(), [&](Vec2 lhs, Vec2 rhs) { return Vec2::distanceSquared(lhs, points.front()) < Vec2::distanceSquared(rhs, points.front()); });
        middle = static_cast<std::size_t>(farthest - points.begin());
    }

    std::vector<bool> kept(line.size(), false);
    kept.front() = true;
    kept[middle] = true;
    kept.back() = true;
    const float toleranceSquared = tolerance * tolerance;
    simplifyRange(line, 0, middle, toleranceSquared, kept);
    simplifyRange(line, middle, line.size() - 1, toleranceSquared, kept);
    if (closed) {
        kept.pop_back();
        line.pop_back();
    }

    // A closed outline thinner than the tolerance keeps the point farthest from the line through its two remaining points.
    if (closed && std::count(kept.begin(), kept.end(), true) < 3) {
        std::size_t extra = 0;
        float farthest = -1.0F;
        for (std::size_t index = 1; index < line.size(); ++index) {
            const float distance = distanceSquaredToSegment(line[index], line.front(), line[middle]);
            if (index != middle && distance > farthest) {
                farthest = distance;
                extra = index;
            }
        }
        kept[extra] = true;
    }

    Outline result;
    for (std::size_t index = 0; index < line.size(); ++index) {
        if (kept[index]) {
            result.push_back(line[index]);
        }
    }
    return result;
}

std::vector<Polygon::Outline> Polygon::decompose(std::span<const Outline> shape, std::size_t maxVertices) {
    if (maxVertices < 3) {
        throw std::invalid_argument("Convex pieces need at least three vertices.");
    }

    const std::vector<Outline> normalized = unite(shape);
    if (normalized.empty()) {
        return {};
    }

    Clipper2Lib::Paths64 triangles;
    if (Clipper2Lib::Triangulate(Clipper::toIntegerPaths(normalized), triangles, true) != Clipper2Lib::TriangulateResult::success) {
        throw std::invalid_argument("The shape could not be triangulated.");
    }

    Merger merger(triangles);
    merger.merge(maxVertices);
    return merger.getPieces();
}

float Polygon::getArea(std::span<const Outline> shape) {
    float area = 0.0F;
    for (const Outline& outline : unite(shape)) {
        area += Geometry::signedArea(outline);
    }
    return area;
}

} // namespace haylen::math
