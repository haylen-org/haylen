#include "3d/physics/jolt/Shape.hpp"

#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/DecoratedShape.h>

#include "3d/physics/jolt/Converter.hpp"

namespace haylen::physics3d::jolt {

const JPH::Shape& Shape::of(const physics3d::Shape& value) {
    return dynamic_cast<const Shape&>(value).getShape();
}

bool Shape::isSolid(const JPH::Shape& shape) noexcept {
    switch (shape.GetType()) {
    case JPH::EShapeType::Convex:
        return true;
    case JPH::EShapeType::Compound: {
        const auto& compound = dynamic_cast<const JPH::CompoundShape&>(shape);
        bool solid = compound.GetNumSubShapes() > 0;
        for (JPH::uint index = 0; index < compound.GetNumSubShapes(); ++index) {
            solid = solid && isSolid(*compound.GetSubShape(index).mShape);
        }

        return solid;
    }
    case JPH::EShapeType::Decorated:
        return isSolid(*dynamic_cast<const JPH::DecoratedShape&>(shape).GetInnerShape());
    default:
        return false;
    }
}

math::Box Shape::getBounds() const {
    return Converter::fromJolt(shape->GetLocalBounds());
}

math::Vec3 Shape::getCenterOfMass() const {
    return Converter::fromJolt(shape->GetCenterOfMass());
}

float Shape::getVolume() const {
    return solid ? shape->GetVolume() : 0.0F;
}

float Shape::getMass() const {
    return solid ? shape->GetMassProperties().mMass : 0.0F;
}

} // namespace haylen::physics3d::jolt
