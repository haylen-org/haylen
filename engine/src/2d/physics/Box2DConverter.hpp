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
    // The linear slop of Box2D in meters, the shortest distance between two points that it tells apart.
    static constexpr float kLinearSlop = 0.005F;

    [[nodiscard]] static b2Vec2 toMeters(math::Vec2 value, float scale) noexcept;
    [[nodiscard]] static math::Vec2 toPixels(b2Vec2 value, float scale) noexcept;
    [[nodiscard]] static b2BodyType toBodyType(Body::Type type) noexcept;
    [[nodiscard]] static b2Filter toFilter(const CollisionFilter& filter) noexcept;
    [[nodiscard]] static b2QueryFilter toQueryFilter(const CollisionFilter& filter) noexcept;
    // Applies the rule of Box2D between a filter and the filter of a shape: a shared group above zero always collides and one below zero never does, and otherwise each category must be in the mask of the other.
    [[nodiscard]] static bool collides(const CollisionFilter& filter, b2Filter shape) noexcept;

    // Throws `std::invalid_argument` for options that Box2D rejects: a density, friction, restitution or rolling resistance that is negative or not finite, or a zero one-way direction.
    static void checkShapeOptions(const Shape::Options& options);

    // Checks the options with `checkShapeOptions` before converting them.
    [[nodiscard]] static b2ShapeDef toShapeDef(const Shape::Options& options);
    [[nodiscard]] static b2SurfaceMaterial toSurfaceMaterial(const Shape::Options& options);

    // Returns the damping of a body, and throws `std::invalid_argument` when it is negative or not finite.
    [[nodiscard]] static float toDamping(float value);

    // Converts a speed in world units per second to meters per second, and throws `std::invalid_argument` when it is negative or not finite.
    [[nodiscard]] static float toSpeed(float value, float scale);

    // Returns the value, and throws `std::invalid_argument` with the message unless it is positive and finite.
    [[nodiscard]] static float toPositive(float value, const char* message);

    // Moves local shape points by the shape offset and rotation and converts them to meters.
    [[nodiscard]] static std::vector<b2Vec2> toLocalPoints(std::span<const math::Vec2> points, const Shape::Options& options, float scale);

    [[nodiscard]] static b2Polygon toHullPolygon(std::span<const b2Vec2> points);

    [[nodiscard]] static bool isFiniteAndNotNegative(float value) noexcept;
};

} // namespace haylen::physics2d
