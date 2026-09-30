#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {
class Random;
}

namespace haylen::tiled {
struct Object;
}

namespace haylen::procedural2d {

// An area to place things in: a rectangle, a circle, a ring or a polygon with optional holes. Random points are uniform over the area.
class Region final {
  public:
    enum class Kind : std::uint8_t {
        Rect,
        Circle,
        Ring,
        Polygon,
    };

    [[nodiscard]] static Region rect(const math::Rect& value);
    [[nodiscard]] static Region circle(math::Vec2 middle, float radius);
    // Throws `std::invalid_argument` when the inner radius is negative or larger than the outer one.
    [[nodiscard]] static Region ring(math::Vec2 middle, float inner, float outer);
    // Takes outlines as `math::Polygon` does, where an outline wound opposite to the one around it is a hole. Throws `std::invalid_argument` when the outlines enclose no area.
    [[nodiscard]] static Region polygon(std::span<const std::vector<math::Vec2>> outlines);
    // Turns a rectangle, ellipse or polygon object of a Tiled map into its area in map pixels, rotation included. Throws `std::invalid_argument` for other shapes.
    [[nodiscard]] static Region fromObject(const tiled::Object& object);

    [[nodiscard]] Kind getKind() const noexcept {
        return kind;
    }
    [[nodiscard]] float getArea() const noexcept {
        return area;
    }
    [[nodiscard]] math::Rect getBounds() const noexcept {
        return bounds;
    }

    [[nodiscard]] bool contains(math::Vec2 point) const noexcept;
    [[nodiscard]] math::Vec2 getRandomPoint(math::Random& random) const noexcept;

  private:
    static constexpr int kEllipseSegments = 48;

    Region() = default;

    [[nodiscard]] math::Vec2 getRandomPolygonPoint(math::Random& random) const noexcept;

    Kind kind = Kind::Rect;
    math::Rect bounds{};
    float area = 0.0F;
    math::Vec2 center{};
    float innerRadius = 0.0F;
    float outerRadius = 0.0F;
    std::vector<std::vector<math::Vec2>> shape;
    std::vector<std::array<math::Vec2, 3>> triangles;
    std::vector<float> cumulativeAreas;
};

} // namespace haylen::procedural2d
