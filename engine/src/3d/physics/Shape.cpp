#include "haylen/3d/physics/Shape.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "haylen/3d/spatial/MeshBvh.hpp"

namespace haylen::physics3d {

const std::array<std::pair<std::string_view, Shape::Kind>, 14> Shape::kKindNames{{
    {"sphere", Kind::Sphere},
    {"box", Kind::Box},
    {"capsule", Kind::Capsule},
    {"taperedCapsule", Kind::TaperedCapsule},
    {"cylinder", Kind::Cylinder},
    {"taperedCylinder", Kind::TaperedCylinder},
    {"convexHull", Kind::ConvexHull},
    {"mesh", Kind::Mesh},
    {"heightField", Kind::HeightField},
    {"plane", Kind::Plane},
    {"staticCompound", Kind::StaticCompound},
    {"mutableCompound", Kind::MutableCompound},
    {"scaled", Kind::Scaled},
    {"offsetCenterOfMass", Kind::OffsetCenterOfMass},
}};

std::optional<Shape::Kind> Shape::kindFromName(std::string_view name) noexcept {
    for (const auto& [text, value] : kKindNames) {
        if (text == name) {
            return value;
        }
    }

    return std::nullopt;
}

std::string_view Shape::kindName(Kind value) noexcept {
    for (const auto& [text, each] : kKindNames) {
        if (each == value) {
            return text;
        }
    }

    return kKindNames.front().first;
}

Shape::Mesh Shape::Mesh::fromBvh(const spatial3d::MeshBvh& bvh) {
    return {.positions = bvh.getPositions(), .indices = bvh.getIndices()};
}

void Shape::requirePositive(std::string_view what, float value) {
    if (!std::isfinite(value) || value <= 0.0F) {
        throw std::invalid_argument(std::format("The {} must be a finite number above zero.", what));
    }
}

void Shape::requireFinite(std::string_view what, math::Vec3 value) {
    if (!value.isFinite()) {
        throw std::invalid_argument(std::format("The {} must be a vector of three finite numbers.", what));
    }
}

// A convex radius rounds the edges inside the shape, so it fits within the smallest half size.
void Shape::requireConvexRadius(std::string_view kind, float value, float largest) {
    if (!std::isfinite(value) || value < 0.0F || value > largest) {
        throw std::invalid_argument(std::format("The convex radius of a {} must be from 0 to {:.6g}, half of its smallest size.", kind, largest));
    }
}

void Shape::requireRotation(std::string_view what, const math::Quat& value) {
    const float length = value.getLength();
    if (!std::isfinite(length) || std::abs(length - 1.0F) > kRotationTolerance) {
        throw std::invalid_argument(std::format("The {} must be a rotation, a quaternion of length one.", what));
    }
}

void Shape::requireChild(std::string_view what, const std::shared_ptr<const Shape>& child, std::string_view backend) {
    if (child == nullptr) {
        throw std::invalid_argument(std::format("The {} needs a shape.", what));
    }

    if (child->getBackendName() != backend) {
        throw std::invalid_argument(std::format("The {} was made by the 3D physics backend \"{}\", and a shape takes only shapes of its own backend \"{}\".", what, child->getBackendName(), backend));
    }
}

void Shape::checkHull(const ConvexHull& hull) {
    if (hull.points.size() < 4 || hull.points.size() > kMaxHullPoints) {
        throw std::invalid_argument(std::format("A convex hull needs from 4 to {} points.", kMaxHullPoints));
    }

    for (const math::Vec3& point : hull.points) {
        requireFinite("point of a convex hull", point);
    }

    if (!spreadsAlongEveryAxis(hull.points)) {
        throw std::invalid_argument("The points of a convex hull lie too close to one plane or one line to hold a volume that the physics builds. Spread them along every axis by at least a millimeter and a thousandth of the length of the hull.");
    }

    if (!std::isfinite(hull.convexRadius) || hull.convexRadius < 0.0F) {
        throw std::invalid_argument("The convex radius of a convex hull must be a finite number of zero or more.");
    }
}

// The points span a length from the first point to the point farthest from it, a width from that line to the point farthest from it and a thickness across the plane of the three, and each must reach the least spread. Every point lies within the length of the first point, so a finite length keeps every distance finite.
bool Shape::spreadsAlongEveryAxis(const std::vector<math::Vec3>& points) {
    const math::Vec3 first = points.front();
    const auto byDistance = [first](math::Vec3 lhs, math::Vec3 rhs) { return math::Vec3::distanceSquared(first, lhs) < math::Vec3::distanceSquared(first, rhs); };
    const math::Vec3 line = *std::ranges::max_element(points, byDistance) - first;
    const float length = line.getLength();
    const float leastSpread = std::max(kMinHullSpread, length * kMinHullSpreadShare);
    if (!std::isfinite(length) || length < leastSpread) {
        return false;
    }

    const math::Vec3 direction = line / length;
    const auto offLine = [first, direction](math::Vec3 point) { return math::Vec3::cross(direction, point - first).getLength(); };
    const auto byLine = [&offLine](math::Vec3 lhs, math::Vec3 rhs) { return offLine(lhs) < offLine(rhs); };
    const math::Vec3 third = *std::ranges::max_element(points, byLine);
    if (offLine(third) < leastSpread) {
        return false;
    }

    const math::Vec3 normal = math::Vec3::cross(direction, third - first).getNormalized();
    const auto acrossPlane = [first, normal](math::Vec3 point) { return math::Vec3::dot(normal, point - first); };
    const auto byPlane = [&acrossPlane](math::Vec3 lhs, math::Vec3 rhs) { return acrossPlane(lhs) < acrossPlane(rhs); };
    const auto [lowest, highest] = std::ranges::minmax_element(points, byPlane);
    return acrossPlane(*highest) - acrossPlane(*lowest) >= leastSpread;
}

void Shape::checkMesh(const Mesh& mesh) {
    const std::size_t vertices = mesh.positions.size() / 3;
    if (mesh.positions.size() % 3 != 0 || vertices > kMaxMeshVertices) {
        throw std::invalid_argument(std::format("A triangle mesh takes three numbers for each vertex, for up to {} vertices.", kMaxMeshVertices));
    }

    for (const float value : mesh.positions) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("The positions of a triangle mesh must be finite numbers.");
        }
    }

    const std::size_t corners = mesh.indices.empty() ? vertices : mesh.indices.size();
    if (corners == 0 || corners % 3 != 0) {
        throw std::invalid_argument("A triangle mesh needs at least one triangle, with three vertices for each triangle.");
    }

    for (const std::uint32_t index : mesh.indices) {
        if (index >= vertices) {
            throw std::invalid_argument(std::format("A triangle of a triangle mesh names the vertex {}, and the mesh has {} vertices.", index, vertices));
        }
    }
}

void Shape::checkHeightField(const HeightField& field) {
    if (field.sampleCount < 3 || field.sampleCount > kMaxHeightFieldSamples) {
        throw std::invalid_argument(std::format("A heightfield needs from 3 to {} samples on each side.", kMaxHeightFieldSamples));
    }

    const auto samples = static_cast<std::size_t>(field.sampleCount) * static_cast<std::size_t>(field.sampleCount);
    if (field.heights.size() != samples) {
        throw std::invalid_argument(std::format("A heightfield of {} samples on each side needs {} heights, and it has {}.", field.sampleCount, samples, field.heights.size()));
    }

    for (const float height : field.heights) {
        if (!std::isfinite(height)) {
            throw std::invalid_argument("The heights of a heightfield must be finite numbers. List the samples without collision in its holes.");
        }
    }

    requireFinite("offset of a heightfield", field.offset);
    requirePositive("scale of a heightfield along x", field.scale.x);
    requirePositive("scale of a heightfield along y", field.scale.y);
    requirePositive("scale of a heightfield along z", field.scale.z);
    for (const std::uint32_t hole : field.holes) {
        if (hole >= samples) {
            throw std::invalid_argument(std::format("A hole of a heightfield names the sample {}, and the heightfield has {} samples.", hole, samples));
        }
    }
}

void Shape::checkChildren(std::string_view kind, const std::vector<Child>& children, std::string_view backend, bool mayBeEmpty) {
    if (children.empty() && !mayBeEmpty) {
        throw std::invalid_argument(std::format("A {} needs at least one child.", kind));
    }

    if (children.size() > kMaxChildren) {
        throw std::invalid_argument(std::format("A {} holds up to {} children.", kind, kMaxChildren));
    }

    for (const Child& child : children) {
        requireChild(std::format("child of a {}", kind), child.shape, backend);
        requireFinite(std::format("position of a child of a {}", kind), child.position);
        requireRotation(std::format("rotation of a child of a {}", kind), child.rotation);
        if (child.shape->getKind() == Kind::Plane) {
            throw std::invalid_argument(std::format("A plane stands alone and is never the child of a {}. Give the plane a body of its own.", kind));
        }
    }
}

void Shape::Settings::validate(std::string_view backend) const {
    requirePositive("density of a shape", density);

    // clang-format off
    std::visit([backend](const auto& shape) {
        using Alternative = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<Alternative, Sphere>) {
            requirePositive("radius of a sphere", shape.radius);
        } else if constexpr (std::is_same_v<Alternative, Box>) {
            requirePositive("width of a box", shape.size.x);
            requirePositive("height of a box", shape.size.y);
            requirePositive("depth of a box", shape.size.z);
            if (shape.convexRadius) {
                requireConvexRadius("box", *shape.convexRadius, std::min({shape.size.x, shape.size.y, shape.size.z}) * 0.5F);
            }
        } else if constexpr (std::is_same_v<Alternative, Capsule>) {
            requirePositive("radius of a capsule", shape.radius);
            requirePositive("height of a capsule", shape.height);
            if (shape.height <= shape.radius * 2.0F) {
                throw std::invalid_argument("The height of a capsule, from the bottom of one cap to the top of the other, must be more than twice its radius. Make a sphere for a shorter one.");
            }
        } else if constexpr (std::is_same_v<Alternative, TaperedCapsule>) {
            requirePositive("top radius of a tapered capsule", shape.topRadius);
            requirePositive("bottom radius of a tapered capsule", shape.bottomRadius);
            requirePositive("height of a tapered capsule", shape.height);
            const float centers = shape.height - shape.topRadius - shape.bottomRadius;
            if (centers <= std::abs(shape.topRadius - shape.bottomRadius)) {
                throw std::invalid_argument("The caps of a tapered capsule must not hold each other. Make it taller than the sum of its radii plus their difference, or make a sphere.");
            }
        } else if constexpr (std::is_same_v<Alternative, Cylinder>) {
            requirePositive("height of a cylinder", shape.height);
            requirePositive("radius of a cylinder", shape.radius);
            if (shape.convexRadius) {
                requireConvexRadius("cylinder", *shape.convexRadius, std::min(shape.height * 0.5F, shape.radius));
            }
        } else if constexpr (std::is_same_v<Alternative, TaperedCylinder>) {
            requirePositive("height of a tapered cylinder", shape.height);
            requirePositive("top radius of a tapered cylinder", shape.topRadius);
            requirePositive("bottom radius of a tapered cylinder", shape.bottomRadius);
            if (shape.convexRadius) {
                requireConvexRadius("tapered cylinder", *shape.convexRadius, std::min({shape.height * 0.5F, shape.topRadius, shape.bottomRadius}));
            }
        } else if constexpr (std::is_same_v<Alternative, ConvexHull>) {
            checkHull(shape);
        } else if constexpr (std::is_same_v<Alternative, Mesh>) {
            checkMesh(shape);
        } else if constexpr (std::is_same_v<Alternative, HeightField>) {
            checkHeightField(shape);
        } else if constexpr (std::is_same_v<Alternative, Plane>) {
            requireFinite("normal of a plane", shape.normal);
            if (shape.normal.getLength() <= kFlatness) {
                throw std::invalid_argument("The normal of a plane must point somewhere, so it cannot be zero.");
            }

            if (!std::isfinite(shape.distance)) {
                throw std::invalid_argument("The distance of a plane must be a finite number.");
            }

            requirePositive("half extent of a plane", shape.halfExtent);
        } else if constexpr (std::is_same_v<Alternative, StaticCompound>) {
            checkChildren("static compound", shape.children, backend, false);
        } else if constexpr (std::is_same_v<Alternative, MutableCompound>) {
            checkChildren("mutable compound", shape.children, backend, true);
        } else if constexpr (std::is_same_v<Alternative, Scaled>) {
            requireChild("shape of a scaled shape", shape.shape, backend);
            requireFinite("scale of a scaled shape", shape.scale);
            if (shape.scale.x == 0.0F || shape.scale.y == 0.0F || shape.scale.z == 0.0F) {
                throw std::invalid_argument("The scale of a scaled shape must not be zero on any axis.");
            }
        } else {
            requireChild("shape of an offset center of mass", shape.shape, backend);
            requireFinite("offset of a center of mass", shape.offset);
        }
    }, geometry);
    // clang-format on
}

} // namespace haylen::physics3d
