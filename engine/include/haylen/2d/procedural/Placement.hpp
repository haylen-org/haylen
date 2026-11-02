#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {
class Random;
}

namespace haylen::procedural2d {

// Places a count of entities of several kinds on a map, such as trees, rocks and houses, each kept apart from the others and from forbidden areas by the shape it really occupies instead of the size of its image. Positions are random within the map with the generator, so the same seed always gives the same map. When the map has no room for everything, it places what fits and tells how many of each kind it left out. A grid of cells holds the placed entities, so every try only measures against its neighbours.
class Placement final {
  public:
    // A circle of the radius, a rectangle of the size, or a simple polygon of the points in either winding, around the center and turned by the rotation. The footprint of an entity lies around the position of the entity, and a forbidden area lies on the map.
    struct Shape {
        enum class Kind : std::uint8_t {
            Circle,
            Rectangle,
            Polygon,
        };

        Kind kind = Kind::Circle;
        math::Vec2 center{};
        float radius = 0.0F;
        math::Vec2 size{};
        std::vector<math::Vec2> points;
        float rotation = 0.0F;
    };

    // How many entities of a kind to place, the footprint each one occupies, the gap it keeps from every other entity and from the forbidden areas, and whether each one turns its footprint by a random angle. Two entities keep the larger gap of their kinds.
    struct Kind {
        std::size_t count = 0;
        Shape footprint;
        float spacing = 0.0F;
        float margin = 0.0F;
        bool rotate = false;
    };

    // The order `Shuffled` places the entities of every kind in a random order, so the kinds share what does not fit, and `Listed` places every entity of a kind before the next kind, so the kinds listed first take the room first.
    enum class Order : std::uint8_t {
        Shuffled,
        Listed,
    };

    // The footprint of every entity lies inside the area, and `attempts` random positions are tried for each entity before it is left out.
    struct Options {
        math::Rect area{};
        std::vector<Kind> kinds;
        std::vector<Shape> forbidden;
        Order order = Order::Shuffled;
        int attempts = 30;
    };

    struct Entity {
        std::uint32_t kind = 0;
        math::Vec2 position{};
        float rotation = 0.0F;
    };

    // The entities in the order they were placed, and for every kind how many of its count found no room.
    struct Result {
        std::vector<Entity> entities;
        std::vector<std::size_t> missing;
    };

    // Throws `std::invalid_argument` for an area without size, a shape without size or with a polygon of fewer than three points, a gap that is negative, attempts below 1, a value that is not finite, or more than `kMaxEntities` entities.
    [[nodiscard]] static Result place(const Options& options, math::Random& random);

    [[nodiscard]] static std::optional<Shape::Kind> shapeKindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view shapeKindName(Shape::Kind value) noexcept;
    [[nodiscard]] static std::optional<Order> orderFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view orderName(Order value) noexcept;

    static constexpr std::size_t kMaxEntities = 1U << 20U;
    static constexpr std::array<std::pair<std::string_view, Shape::Kind>, 3> kShapeKindNames{{{"circle", Shape::Kind::Circle}, {"rectangle", Shape::Kind::Rectangle}, {"polygon", Shape::Kind::Polygon}}};
    static constexpr std::array<std::pair<std::string_view, Order>, 2> kOrderNames{{{"shuffled", Order::Shuffled}, {"listed", Order::Listed}}};

  private:
    class Placer;

    // A grid of this many cells at most holds the placed entities.
    static constexpr std::size_t kMaxCells = 1U << 22U;

    static void validate(const Options& options);
    static void validateShape(const Shape& shape);
};

} // namespace haylen::procedural2d
