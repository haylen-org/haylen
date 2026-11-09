#include "haylen/2d/procedural/Placement.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Segment.hpp"

namespace haylen::procedural2d {

// Tries the entities one by one against the forbidden areas and the entities already placed, which a grid of cells finds around every try by their positions. A placed shape is a circle or the points of a polygon in map coordinates, with a circle around its anchor that holds it, which rejects far shapes without measuring them.
class Placement::Placer final {
  public:
    Placer(const Options& placement, math::Random& generator) : options(placement), random(generator) {
        // Two entities of any kinds can only come too close while their anchors lie within the largest bound twice and the widest gap, which a cell spans, so a try only measures against the cells around it.
        float bound = 0.0F;
        float spacing = 0.0F;
        for (const Kind& kind : options.kinds) {
            bound = std::max(bound, getBound(kind.footprint));
            spacing = std::max(spacing, kind.spacing);
        }
        const float reach = bound * 2.0F + spacing;

        // A cell spans at least the longer side divided by the most cells on a side, so even a long thin area keeps the grid within its bound of cells.
        const math::Rect& area = options.area;
        const float least = std::max(area.width, area.height) / static_cast<float>(kMaxSide);
        cell = std::max({reach, least, 1.0e-3F});
        columns = std::max(1, static_cast<int>(std::ceil(area.width / cell)));
        rows = std::max(1, static_cast<int>(std::ceil(area.height / cell)));
        heads.assign(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows), kNone);

        for (const Shape& shape : options.forbidden) {
            Solid zone = build(shape, {}, 0.0F, 0.0F);
            zone.anchor = zone.center;
            measure(zone);
            forbidden.push_back(zone);
        }
    }

    // Places one entity of the kind, or returns false when no try finds room for it.
    bool place(std::uint32_t kindIndex, std::vector<Entity>& entities) {
        const Kind& kind = options.kinds[kindIndex];
        const math::Rect& area = options.area;
        for (int attempt = 0; attempt < options.attempts; ++attempt) {
            const math::Vec2 position{random.range(area.getLeft(), area.getRight()), random.range(area.getTop(), area.getBottom())};
            const float angle = kind.rotate ? random.range(0.0F, math::Math::kTau) : 0.0F;
            const std::size_t mark = points.size();
            const Solid candidate = build(kind.footprint, position, angle, kind.spacing);
            if (!fits(candidate, kind)) {
                points.resize(mark);
                continue;
            }

            const auto index = static_cast<std::uint32_t>(solids.size());
            const std::size_t slot = getCell(candidate.anchor);
            solids.push_back(candidate);
            next.push_back(heads[slot]);
            heads[slot] = index;
            entities.push_back({.kind = kindIndex, .position = position, .rotation = angle});
            return true;
        }
        return false;
    }

  private:
    static constexpr std::uint32_t kNone = std::numeric_limits<std::uint32_t>::max();

    // A shape in map coordinates: a circle of the radius around the center, or the run of `count` points from `first` in the points of the placer, inside the circle of `bound` around the anchor, the position of an entity or the center of a forbidden area.
    struct Solid {
        bool circle = true;
        math::Vec2 anchor{};
        float bound = 0.0F;
        math::Vec2 center{};
        float radius = 0.0F;
        std::uint32_t first = 0;
        std::uint32_t count = 0;
        float spacing = 0.0F;
    };

    // Returns the radius of the circle around the position of an entity that holds its footprint at any rotation.
    [[nodiscard]] static float getBound(const Shape& shape) noexcept {
        float extent = shape.radius;
        if (shape.kind == Shape::Kind::Rectangle) {
            extent = shape.size.getLength() * 0.5F;
        } else if (shape.kind == Shape::Kind::Polygon) {
            extent = 0.0F;
            for (const math::Vec2 point : shape.points) {
                extent = std::max(extent, point.getLength());
            }
        }
        return shape.center.getLength() + extent;
    }

    [[nodiscard]] static math::Vec2 turn(math::Vec2 point, float angle) noexcept {
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        return {point.x * cosine - point.y * sine, point.x * sine + point.y * cosine};
    }

    // Places the shape around the position, turned by the angle around it, and adds the points of a rectangle or a polygon to the points of the placer.
    [[nodiscard]] Solid build(const Shape& shape, math::Vec2 position, float angle, float spacing) {
        Solid solid{.anchor = position, .center = position + turn(shape.center, angle), .radius = shape.radius, .spacing = spacing};
        if (shape.kind == Shape::Kind::Circle) {
            measure(solid);
            return solid;
        }

        solid.circle = false;
        solid.first = static_cast<std::uint32_t>(points.size());
        const float turned = shape.rotation + angle;
        if (shape.kind == Shape::Kind::Rectangle) {
            const math::Vec2 half = shape.size * 0.5F;
            for (const math::Vec2 corner : {math::Vec2{-half.x, -half.y}, math::Vec2{half.x, -half.y}, math::Vec2{half.x, half.y}, math::Vec2{-half.x, half.y}}) {
                points.push_back(solid.center + turn(corner, turned));
            }
        } else {
            for (const math::Vec2 point : shape.points) {
                points.push_back(solid.center + turn(point, turned));
            }
        }
        solid.count = static_cast<std::uint32_t>(points.size()) - solid.first;
        measure(solid);
        return solid;
    }

    // Sets the radius of the circle around the anchor that holds the shape.
    void measure(Solid& solid) const noexcept {
        if (solid.circle) {
            solid.bound = math::Vec2::distance(solid.center, solid.anchor) + solid.radius;
            return;
        }
        solid.bound = 0.0F;
        for (const math::Vec2 point : getPoints(solid)) {
            solid.bound = std::max(solid.bound, math::Vec2::distance(point, solid.anchor));
        }
    }

    [[nodiscard]] std::span<const math::Vec2> getPoints(const Solid& solid) const noexcept {
        return std::span(points).subspan(solid.first, solid.count);
    }

    [[nodiscard]] std::size_t getCell(math::Vec2 point) const noexcept {
        const int column = std::clamp(static_cast<int>((point.x - options.area.x) / cell), 0, columns - 1);
        const int row = std::clamp(static_cast<int>((point.y - options.area.y) / cell), 0, rows - 1);
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(column);
    }

    // The footprint lies inside the area, keeps the margin from every forbidden area and keeps the larger gap of the two kinds from every entity of the cells around it.
    [[nodiscard]] bool fits(const Solid& candidate, const Kind& kind) const {
        if (!isInside(candidate)) {
            return false;
        }
        for (const Solid& area : forbidden) {
            if (!isApart(candidate, area, kind.margin)) {
                return false;
            }
        }

        const int column = static_cast<int>((candidate.anchor.x - options.area.x) / cell);
        const int row = static_cast<int>((candidate.anchor.y - options.area.y) / cell);
        for (int y = std::max(row - 1, 0); y <= std::min(row + 1, rows - 1); ++y) {
            for (int x = std::max(column - 1, 0); x <= std::min(column + 1, columns - 1); ++x) {
                for (std::uint32_t index = heads[static_cast<std::size_t>(y) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(x)]; index != kNone; index = next[index]) {
                    const Solid& other = solids[index];
                    if (!isApart(candidate, other, std::max(candidate.spacing, other.spacing))) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool isInside(const Solid& solid) const noexcept {
        const math::Rect& area = options.area;
        if (solid.circle) {
            return solid.center.x - solid.radius >= area.getLeft() && solid.center.x + solid.radius <= area.getRight() && solid.center.y - solid.radius >= area.getTop() && solid.center.y + solid.radius <= area.getBottom();
        }
        return std::ranges::all_of(getPoints(solid), [&area](math::Vec2 point) { return point.x >= area.getLeft() && point.x <= area.getRight() && point.y >= area.getTop() && point.y <= area.getBottom(); });
    }

    // Tells whether two shapes stay at least the gap apart, where shapes that overlap or hold one another never do.
    [[nodiscard]] bool isApart(const Solid& first, const Solid& second, float gap) const {
        if (math::Vec2::distance(first.anchor, second.anchor) >= first.bound + second.bound + gap) {
            return true;
        }
        if (first.circle && second.circle) {
            return math::Vec2::distance(first.center, second.center) >= first.radius + second.radius + gap;
        }
        if (first.circle || second.circle) {
            const Solid& round = first.circle ? first : second;
            const std::span<const math::Vec2> outline = getPoints(first.circle ? second : first);
            return !math::Geometry::contains(outline, round.center) && getDistance(outline, round.center) >= round.radius + gap;
        }

        const std::span<const math::Vec2> lhs = getPoints(first);
        const std::span<const math::Vec2> rhs = getPoints(second);
        if (math::Geometry::contains(lhs, rhs.front()) || math::Geometry::contains(rhs, lhs.front()) || crosses(lhs, rhs)) {
            return false;
        }
        return std::ranges::all_of(lhs, [&rhs, gap](math::Vec2 point) { return getDistance(rhs, point) >= gap; }) && std::ranges::all_of(rhs, [&lhs, gap](math::Vec2 point) { return getDistance(lhs, point) >= gap; });
    }

    [[nodiscard]] static float getDistance(std::span<const math::Vec2> outline, math::Vec2 point) noexcept {
        float nearest = std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < outline.size(); ++index) {
            nearest = std::min(nearest, math::Geometry::distanceToSegment({outline[index], outline[(index + 1) % outline.size()]}, point));
        }
        return nearest;
    }

    [[nodiscard]] static bool crosses(std::span<const math::Vec2> lhs, std::span<const math::Vec2> rhs) noexcept {
        for (std::size_t first = 0; first < lhs.size(); ++first) {
            const math::Segment edge{lhs[first], lhs[(first + 1) % lhs.size()]};
            for (std::size_t second = 0; second < rhs.size(); ++second) {
                if (math::Geometry::intersection(edge, {rhs[second], rhs[(second + 1) % rhs.size()]})) {
                    return true;
                }
            }
        }
        return false;
    }

    const Options& options;
    math::Random& random;
    float cell = 1.0F;
    int columns = 1;
    int rows = 1;

    // The first entity of every cell, the entity after each one in its cell, the shapes of the placed entities and of the forbidden areas, and the points of their polygons.
    std::vector<std::uint32_t> heads;
    std::vector<std::uint32_t> next;
    std::vector<Solid> solids;
    std::vector<Solid> forbidden;
    std::vector<math::Vec2> points;
};

std::optional<Placement::Shape::Kind> Placement::shapeKindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kShapeKindNames, name, &std::pair<std::string_view, Shape::Kind>::first);
    return found != kShapeKindNames.end() ? std::optional<Shape::Kind>(found->second) : std::nullopt;
}

std::string_view Placement::shapeKindName(Shape::Kind value) noexcept {
    const auto found = std::ranges::find(kShapeKindNames, value, &std::pair<std::string_view, Shape::Kind>::second);
    return found != kShapeKindNames.end() ? found->first : std::string_view{};
}

std::optional<Placement::Order> Placement::orderFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kOrderNames, name, &std::pair<std::string_view, Order>::first);
    return found != kOrderNames.end() ? std::optional<Order>(found->second) : std::nullopt;
}

std::string_view Placement::orderName(Order value) noexcept {
    const auto found = std::ranges::find(kOrderNames, value, &std::pair<std::string_view, Order>::second);
    return found != kOrderNames.end() ? found->first : std::string_view{};
}

void Placement::validateShape(const Shape& shape) {
    const bool finite = std::isfinite(shape.center.x) && std::isfinite(shape.center.y) && std::isfinite(shape.rotation) && std::ranges::all_of(shape.points, [](math::Vec2 point) { return std::isfinite(point.x) && std::isfinite(point.y); });
    bool sized = false;
    switch (shape.kind) {
    case Shape::Kind::Circle:
        sized = shape.radius > 0.0F && std::isfinite(shape.radius);
        break;
    case Shape::Kind::Rectangle:
        sized = shape.size.x > 0.0F && shape.size.y > 0.0F && std::isfinite(shape.size.x) && std::isfinite(shape.size.y);
        break;
    case Shape::Kind::Polygon:
        sized = shape.points.size() >= 3 && math::Geometry::signedArea(shape.points) != 0.0F;
        break;
    }
    if (!finite || !sized) {
        throw std::invalid_argument("A placement shape needs finite values and a size: a positive radius, a positive width and height, or a polygon of at least three points around an area.");
    }
}

void Placement::validate(const Options& options) {
    const math::Rect& area = options.area;
    if (!(area.width > 0.0F && area.height > 0.0F) || !std::isfinite(area.x) || !std::isfinite(area.y) || !std::isfinite(area.width) || !std::isfinite(area.height)) {
        throw std::invalid_argument("A placement needs a finite area with a positive width and height.");
    }
    if (options.attempts < 1) {
        throw std::invalid_argument("A placement needs at least one attempt for each entity.");
    }

    std::size_t total = 0;
    for (const Kind& kind : options.kinds) {
        validateShape(kind.footprint);
        if (!(kind.spacing >= 0.0F && kind.margin >= 0.0F) || !std::isfinite(kind.spacing) || !std::isfinite(kind.margin)) {
            throw std::invalid_argument("A placement kind needs a finite spacing and margin of zero or more.");
        }
        total += std::min(kind.count, kMaxEntities + 1);
    }
    if (total > kMaxEntities) {
        throw std::invalid_argument("A placement places at most 1048576 entities.");
    }
    for (const Shape& shape : options.forbidden) {
        validateShape(shape);
    }
}

Placement::Result Placement::place(const Options& options, math::Random& random) {
    validate(options);
    Result result;
    result.missing.assign(options.kinds.size(), 0);

    std::vector<std::uint32_t> order;
    for (std::size_t kind = 0; kind < options.kinds.size(); ++kind) {
        order.insert(order.end(), options.kinds[kind].count, static_cast<std::uint32_t>(kind));
    }
    if (options.order == Order::Shuffled) {
        random.shuffle(std::span(order));
    }

    Placer placer(options, random);
    result.entities.reserve(order.size());
    for (const std::uint32_t kind : order) {
        if (!placer.place(kind, result.entities)) {
            ++result.missing[kind];
        }
    }
    return result;
}

} // namespace haylen::procedural2d
