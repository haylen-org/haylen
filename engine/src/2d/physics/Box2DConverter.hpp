#pragma once

#include <box2d/box2d.h>

#include <span>
#include <vector>

#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::physics2d {

// Converts between world units and engine types and the meters and definitions of Box2D.
class Box2DConverter final {
  public:
    [[nodiscard]] static b2Vec2 toMeters(math::Vec2 value, float scale) noexcept;
    [[nodiscard]] static math::Vec2 toPixels(b2Vec2 value, float scale) noexcept;
    [[nodiscard]] static b2BodyType toBodyType(Body::Type type) noexcept;
    [[nodiscard]] static b2Filter toFilter(const CollisionFilter& filter) noexcept;
    [[nodiscard]] static b2QueryFilter toQueryFilter(const CollisionFilter& filter) noexcept;
    [[nodiscard]] static b2ShapeDef toShapeDef(const Shape::Options& options) noexcept;

    // Moves local shape points by the shape offset and rotation and converts them to meters.
    [[nodiscard]] static std::vector<b2Vec2> toLocalPoints(std::span<const math::Vec2> points, const Shape::Options& options, float scale);

    [[nodiscard]] static b2Polygon toHullPolygon(std::span<const b2Vec2> points);
};

} // namespace haylen::physics2d
