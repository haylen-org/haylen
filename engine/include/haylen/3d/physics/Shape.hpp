#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "haylen/math/Box.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::spatial3d {
class MeshBvh;
}

namespace haylen::physics3d {

// The collision shape of 3D bodies, in meters, which the backend of the worlds makes from `Shape::Settings`. A shape never changes once it is made, so many bodies of any world of its backend share one, and it lives while a body or a compound holds it. Every round shape stands along `y`, so a capsule or a cylinder stands upright.
class Shape {
  public:
    static constexpr float kDefaultConvexRadius = 0.05F;

    enum class Kind : std::uint8_t {
        Sphere,
        Box,
        Capsule,
        TaperedCapsule,
        Cylinder,
        TaperedCylinder,
        ConvexHull,
        Mesh,
        HeightField,
        Plane,
        StaticCompound,
        MutableCompound,
        Scaled,
        OffsetCenterOfMass,
    };

    // The convex radius rounds the edges of boxes, cylinders and hulls, which makes their collisions faster and steadier, and it never grows them. An unset radius of a box or a cylinder is `kDefaultConvexRadius`, or half of its smallest size when that is less, and the radius of a hull is the most it takes, which shrinks to fit the hull.
    struct Sphere {
        float radius = 0.5F;
    };
    struct Box {
        math::Vec3 size{1.0F, 1.0F, 1.0F};
        std::optional<float> convexRadius{};
    };

    // The height of a capsule runs from the bottom of its lower cap to the top of its upper cap.
    struct Capsule {
        float height = 2.0F;
        float radius = 0.5F;
    };
    struct TaperedCapsule {
        float height = 2.0F;
        float topRadius = 0.3F;
        float bottomRadius = 0.5F;
    };
    struct Cylinder {
        float height = 1.0F;
        float radius = 0.5F;
        std::optional<float> convexRadius{};
    };
    struct TaperedCylinder {
        float height = 1.0F;
        float topRadius = 0.3F;
        float bottomRadius = 0.5F;
        std::optional<float> convexRadius{};
    };

    // The smallest convex solid that holds every point.
    struct ConvexHull {
        std::vector<math::Vec3> points{};
        float convexRadius = kDefaultConvexRadius;
    };

    // The triangles of a surface, with three numbers for each vertex and three vertices for each triangle, or no indices for positions that list every triangle in turn, whose corners run counter-clockwise on their front. Triangles without area are left out. Only static and kinematic bodies take a mesh.
    struct Mesh {
        std::vector<float> positions{};
        std::vector<std::uint32_t> indices{};

        // Takes the positions and triangles of a mesh tree.
        [[nodiscard]] static Mesh fromBvh(const spatial3d::MeshBvh& bvh);
    };

    // A terrain of `sampleCount` by `sampleCount` heights in rows along `z`, so the height of column `x` and row `z` is at `z * sampleCount + x`. A sample lies at `offset + scale * (x, height, z)`, and the holes list the samples without collision. Only static and kinematic bodies take a heightfield.
    struct HeightField {
        std::vector<float> heights{};
        int sampleCount = 0;
        math::Vec3 scale{1.0F, 1.0F, 1.0F};
        math::Vec3 offset{};
        std::vector<std::uint32_t> holes{};
    };

    // The half of the space under a plane, whose normal points out of it, which collides within a cube of twice `halfExtent` meters around the origin of the shape. Only static and kinematic bodies take a plane.
    struct Plane {
        math::Vec3 normal{0.0F, 1.0F, 0.0F};
        float distance = 0.0F;
        float halfExtent = 1000.0F;
    };

    // A part of a compound, placed in the space of the compound.
    struct Child {
        std::shared_ptr<const Shape> shape{};
        math::Vec3 position{};
        math::Quat rotation = math::Quat::identity();
    };

    // A static compound builds a tree over its children once. A mutable compound gives each body that takes it its own copy, whose children the body adds, removes and moves while it lives.
    struct StaticCompound {
        std::vector<Child> children{};
    };
    struct MutableCompound {
        std::vector<Child> children{};
    };

    // Spheres and capsules take the same scale on every axis, and cylinders the same scale on `x` and `z`. A negative scale mirrors the shape.
    struct Scaled {
        std::shared_ptr<const Shape> shape{};
        math::Vec3 scale{1.0F, 1.0F, 1.0F};
    };

    // Moves the center of mass of the shape by the offset without moving the shape, such as the low center of mass of a car that stays on its wheels.
    struct OffsetCenterOfMass {
        std::shared_ptr<const Shape> shape{};
        math::Vec3 offset{};
    };

    using Geometry = std::variant<Sphere, Box, Capsule, TaperedCapsule, Cylinder, TaperedCylinder, ConvexHull, Mesh, HeightField, Plane, StaticCompound, MutableCompound, Scaled, OffsetCenterOfMass>;

    // The density gives a shape its mass in kilograms per cubic meter, 1000 by default, the density of water.
    struct Settings {
        Geometry geometry = Sphere{};
        float density = 1000.0F;

        [[nodiscard]] Kind getKind() const noexcept {
            return static_cast<Kind>(geometry.index());
        }

        // Throws `std::invalid_argument` with a sentence that says what the shape takes, for a size that is not positive, a value that is not finite, a mesh whose indices pass its vertices, a hull of fewer than four points or of points that do not spread along every axis by `kMinHullSpread` and `kMinHullSpreadShare` of its length, a heightfield whose heights do not fill its grid and a compound without children or with a child of another backend than `backend`.
        void validate(std::string_view backend) const;
    };

    // The most points of a hull, vertices of a mesh and samples on a side of a heightfield, far beyond what a device holds.
    static constexpr std::size_t kMaxHullPoints = 1 << 16;
    static constexpr std::size_t kMaxMeshVertices = 1 << 24;
    static constexpr int kMaxHeightFieldSamples = 4096;

    // The least spread of the points of a hull along every axis, in meters and as a share of the length of the hull. The physics places every point on a face of a hull within a millimeter and computes the directions of the faces in floats, so a hull thinner than either loses points between its faces.
    static constexpr float kMinHullSpread = 1.0e-3F;
    static constexpr float kMinHullSpreadShare = 1.0e-3F;

    explicit Shape(Kind shapeKind) noexcept : kind(shapeKind) {}
    virtual ~Shape() = default;

    Shape(const Shape&) = delete;
    Shape& operator=(const Shape&) = delete;

    [[nodiscard]] static std::optional<Kind> kindFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;

    [[nodiscard]] Kind getKind() const noexcept {
        return kind;
    }

    [[nodiscard]] virtual std::string_view getBackendName() const noexcept = 0;

    // The box that holds the shape in its own space.
    [[nodiscard]] virtual math::Box getBounds() const = 0;

    // The center of mass in the space of the shape, the volume in cubic meters and the mass in kilograms that the density gives it. A mesh, a heightfield and a plane have no volume and no mass.
    [[nodiscard]] virtual math::Vec3 getCenterOfMass() const = 0;
    [[nodiscard]] virtual float getVolume() const = 0;
    [[nodiscard]] virtual float getMass() const = 0;

    // Whether a dynamic body takes the shape, which every shape with a volume does, while a mesh, a heightfield, a plane and a compound that holds one of them do not.
    [[nodiscard]] virtual bool isSolid() const noexcept = 0;

  private:
    static const std::array<std::pair<std::string_view, Kind>, 14> kKindNames;

    // A compound holds up to this many children, and a rotation may be off a length of one by this much.
    static constexpr std::size_t kMaxChildren = 1 << 16;
    static constexpr float kRotationTolerance = 1.0e-3F;

    // A normal of a plane shorter than this has no direction.
    static constexpr float kFlatness = 1.0e-6F;

    static void requirePositive(std::string_view what, float value);
    static void requireFinite(std::string_view what, math::Vec3 value);
    static void requireConvexRadius(std::string_view kind, float value, float largest);
    static void requireRotation(std::string_view what, const math::Quat& value);
    static void requireChild(std::string_view what, const std::shared_ptr<const Shape>& child, std::string_view backend);
    static void checkHull(const ConvexHull& hull);
    [[nodiscard]] static bool spreadsAlongEveryAxis(const std::vector<math::Vec3>& points);
    static void checkMesh(const Mesh& mesh);
    static void checkHeightField(const HeightField& field);
    static void checkChildren(std::string_view kind, const std::vector<Child>& children, std::string_view backend, bool mayBeEmpty);

    Kind kind;
};

} // namespace haylen::physics3d
