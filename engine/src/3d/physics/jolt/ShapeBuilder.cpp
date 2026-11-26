#include "3d/physics/jolt/ShapeBuilder.hpp"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCylinderShape.h>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "3d/physics/jolt/Converter.hpp"
#include "3d/physics/jolt/Shape.hpp"

namespace haylen::physics3d::jolt {

using Settings = physics3d::Shape::Settings;

JPH::RefConst<JPH::Shape> ShapeBuilder::make(const JPH::ShapeSettings& settings, std::string_view failure) {
    const JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) {
        throw std::invalid_argument(std::string(failure));
    }

    return result.Get();
}

JPH::RefConst<JPH::Shape> ShapeBuilder::makeHull(const physics3d::Shape::ConvexHull& hull, float density) {
    JPH::Array<JPH::Vec3> points;
    points.reserve(hull.points.size());
    for (const math::Vec3 point : hull.points) {
        points.push_back(Converter::toJolt(point));
    }

    JPH::ConvexHullShapeSettings settings(points, hull.convexRadius);
    settings.SetEmbedded();
    settings.SetDensity(density);
    return make(settings, "The points of a convex hull make no solid that the physics holds, such as points that nearly lie on one plane. Give it points that spread along every axis.");
}

// Making the settings leaves out the triangles without area and the triangles that repeat.
JPH::RefConst<JPH::Shape> ShapeBuilder::makeMesh(const physics3d::Shape::Mesh& mesh) {
    JPH::VertexList vertices;
    vertices.reserve(mesh.positions.size() / 3);
    for (std::size_t base = 0; base < mesh.positions.size(); base += 3) {
        vertices.emplace_back(mesh.positions[base], mesh.positions[base + 1], mesh.positions[base + 2]);
    }

    const std::size_t corners = mesh.indices.empty() ? vertices.size() : mesh.indices.size();
    JPH::IndexedTriangleList triangles;
    triangles.reserve(corners / 3);
    for (std::size_t corner = 0; corner < corners; corner += 3) {
        if (mesh.indices.empty()) {
            const auto first = static_cast<JPH::uint32>(corner);
            triangles.emplace_back(first, first + 1, first + 2, 0);
        } else {
            triangles.emplace_back(mesh.indices[corner], mesh.indices[corner + 1], mesh.indices[corner + 2], 0);
        }
    }

    JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
    settings.SetEmbedded();
    if (settings.mIndexedTriangles.empty()) {
        throw std::invalid_argument("A triangle mesh needs at least one triangle with an area, and every triangle of this one has its corners on one line.");
    }

    return make(settings, "A triangle mesh holds more triangles than the physics takes in one shape. Split it into several bodies.");
}

JPH::RefConst<JPH::Shape> ShapeBuilder::makeHeightField(const physics3d::Shape::HeightField& field) {
    std::vector<float> heights = field.heights;
    for (const std::uint32_t hole : field.holes) {
        heights[hole] = JPH::HeightFieldShapeConstants::cNoCollisionValue;
    }

    JPH::HeightFieldShapeSettings settings(heights.data(), Converter::toJolt(field.offset), Converter::toJolt(field.scale), static_cast<JPH::uint32>(field.sampleCount));
    settings.SetEmbedded();
    return make(settings, "A heightfield holds more samples than the physics takes in one shape. Split it into several bodies.");
}

JPH::RefConst<JPH::Shape> ShapeBuilder::makeStaticCompound(const std::vector<physics3d::Shape::Child>& children) {
    JPH::StaticCompoundShapeSettings settings;
    settings.SetEmbedded();
    for (const physics3d::Shape::Child& child : children) {
        settings.AddShape(Converter::toJolt(child.position), Converter::toJolt(child.rotation.getNormalized()), &Shape::of(*child.shape));
    }

    return make(settings, "A static compound nests more compounds inside each other than the physics takes. Flatten the children into fewer levels.");
}

JPH::RefConst<JPH::Shape> ShapeBuilder::makeMutableCompound(const std::vector<physics3d::Shape::Child>& children) {
    JPH::MutableCompoundShapeSettings settings;
    settings.SetEmbedded();
    for (const physics3d::Shape::Child& child : children) {
        settings.AddShape(Converter::toJolt(child.position), Converter::toJolt(child.rotation.getNormalized()), &Shape::of(*child.shape));
    }

    return make(settings, "A mutable compound nests more compounds inside each other than the physics takes. Flatten the children into fewer levels.");
}

std::shared_ptr<const Shape> ShapeBuilder::build(const Settings& settings) {
    const float density = settings.density;

    // clang-format off
    const JPH::RefConst<JPH::Shape> made = std::visit([density](const auto& shape) -> JPH::RefConst<JPH::Shape> {
        using Alternative = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<Alternative, physics3d::Shape::Sphere>) {
            JPH::SphereShapeSettings sphere(shape.radius);
            sphere.SetEmbedded();
            sphere.SetDensity(density);
            return make(sphere, "A sphere of this radius makes no shape that the physics holds.");
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::Box>) {
            const float smallest = std::min({shape.size.x, shape.size.y, shape.size.z}) * 0.5F;
            JPH::BoxShapeSettings box(Converter::toJolt(shape.size * 0.5F), shape.convexRadius.value_or(std::min(physics3d::Shape::kDefaultConvexRadius, smallest)));
            box.SetEmbedded();
            box.SetDensity(density);
            return make(box, "A box of this size makes no shape that the physics holds.");
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::Capsule>) {
            JPH::CapsuleShapeSettings capsule(shape.height * 0.5F - shape.radius, shape.radius);
            capsule.SetEmbedded();
            capsule.SetDensity(density);
            return make(capsule, "A capsule of this size makes no shape that the physics holds.");
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::TaperedCapsule>) {
            JPH::TaperedCapsuleShapeSettings capsule((shape.height - shape.topRadius - shape.bottomRadius) * 0.5F, shape.topRadius, shape.bottomRadius);
            capsule.SetEmbedded();
            capsule.SetDensity(density);
            return make(capsule, "A tapered capsule of this size makes no shape that the physics holds.");
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::Cylinder>) {
            const float smallest = std::min(shape.height * 0.5F, shape.radius);
            JPH::CylinderShapeSettings cylinder(shape.height * 0.5F, shape.radius, shape.convexRadius.value_or(std::min(physics3d::Shape::kDefaultConvexRadius, smallest)));
            cylinder.SetEmbedded();
            cylinder.SetDensity(density);
            return make(cylinder, "A cylinder of this size makes no shape that the physics holds.");
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::TaperedCylinder>) {
            const float smallest = std::min({shape.height * 0.5F, shape.topRadius, shape.bottomRadius});
            JPH::TaperedCylinderShapeSettings cylinder(shape.height * 0.5F, shape.topRadius, shape.bottomRadius, shape.convexRadius.value_or(std::min(physics3d::Shape::kDefaultConvexRadius, smallest)));
            cylinder.SetEmbedded();
            cylinder.SetDensity(density);
            return make(cylinder, "A tapered cylinder of this size makes no shape that the physics holds.");
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::ConvexHull>) {
            return makeHull(shape, density);
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::Mesh>) {
            return makeMesh(shape);
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::HeightField>) {
            return makeHeightField(shape);
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::Plane>) {
            const JPH::Vec3 normal = Converter::toJolt(shape.normal).Normalized();
            JPH::PlaneShapeSettings plane(JPH::Plane(normal, -shape.distance), nullptr, shape.halfExtent);
            plane.SetEmbedded();
            return make(plane, "A plane of this size makes no shape that the physics holds.");
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::StaticCompound>) {
            return makeStaticCompound(shape.children);
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::MutableCompound>) {
            return makeMutableCompound(shape.children);
        } else if constexpr (std::is_same_v<Alternative, physics3d::Shape::Scaled>) {
            const JPH::Shape& inner = Shape::of(*shape.shape);
            if (!inner.IsValidScale(Converter::toJolt(shape.scale))) {
                throw std::invalid_argument("The scale does not suit the shape it scales. Spheres and capsules take the same scale on every axis, cylinders the same scale on x and z, and the children of a compound the scales their own shapes take.");
            }

            JPH::ScaledShapeSettings scaled(&inner, Converter::toJolt(shape.scale));
            scaled.SetEmbedded();
            return make(scaled, "The scale makes no shape that the physics holds.");
        } else {
            JPH::OffsetCenterOfMassShapeSettings offset(Converter::toJolt(shape.offset), &Shape::of(*shape.shape));
            offset.SetEmbedded();
            return make(offset, "The offset of the center of mass makes no shape that the physics holds.");
        }
    }, settings.geometry);
    // clang-format on

    return std::make_shared<const Shape>(settings.getKind(), made, Shape::isSolid(*made));
}

} // namespace haylen::physics3d::jolt
