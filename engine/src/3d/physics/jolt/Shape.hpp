#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <string_view>
#include <utility>

#include "3d/physics/jolt/Backend.hpp"
#include "haylen/3d/physics/Shape.hpp"

namespace haylen::physics3d::jolt {

// A shape of Jolt behind the shape of the engine. The shape of Jolt never changes once made, so worlds on any thread share it.
class Shape final : public physics3d::Shape {
  public:
    Shape(Kind shapeKind, JPH::RefConst<JPH::Shape> made, bool solidShape) noexcept : physics3d::Shape(shapeKind), shape(std::move(made)), solid(solidShape) {}

    // Returns the shape of Jolt behind a shape that this backend made, which `Shape::Settings::validate` and `World` check before.
    [[nodiscard]] static const JPH::Shape& of(const physics3d::Shape& value);

    // A shape of Jolt is solid when it has a volume: a convex shape, a compound of solid shapes that holds one at least, or a solid shape that another moves, turns or scales.
    [[nodiscard]] static bool isSolid(const JPH::Shape& shape) noexcept;

    [[nodiscard]] std::string_view getBackendName() const noexcept override {
        return Backend::kName;
    }
    [[nodiscard]] math::Box getBounds() const override;
    [[nodiscard]] math::Vec3 getCenterOfMass() const override;
    [[nodiscard]] float getVolume() const override;
    [[nodiscard]] float getMass() const override;
    [[nodiscard]] bool isSolid() const noexcept override {
        return solid;
    }

    [[nodiscard]] const JPH::Shape& getShape() const noexcept {
        return *shape;
    }

  private:
    JPH::RefConst<JPH::Shape> shape;
    bool solid = true;
};

} // namespace haylen::physics3d::jolt
