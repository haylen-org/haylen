#include "haylen/3d/spatial/MeshBvh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

#include "3d/spatial/EntryBounds.hpp"
#include "haylen/math/Geometry3D.hpp"

namespace haylen::spatial3d {

MeshBvh::MeshBvh(std::vector<float> vertexPositions, std::vector<std::uint32_t> triangleIndices) : positions(std::move(vertexPositions)), indices(std::move(triangleIndices)) {
    validate();

    const std::size_t triangleCount = indices.empty() ? getVertexCount() / 3 : indices.size() / 3;
    std::vector<math::Box> boxes;
    boxes.reserve(triangleCount);
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const math::Triangle corners = getCorners(static_cast<std::uint32_t>(triangle));
        boxes.push_back(math::Box{corners.a, corners.a}.merged(corners.b).merged(corners.c));
    }

    tree.build(boxes, order);
}

void MeshBvh::validate() const {
    if (positions.size() % 3 != 0) {
        throw std::invalid_argument("The positions of a mesh need three numbers for each vertex.");
    }

    if (!std::ranges::all_of(positions, [](float value) { return std::isfinite(value); })) {
        throw std::invalid_argument("Every position of a mesh must be a finite number.");
    }

    const std::size_t vertexCount = getVertexCount();
    if (vertexCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("A mesh holds at most 4294967295 vertices.");
    }

    if (indices.empty()) {
        if (vertexCount % 3 != 0) {
            throw std::invalid_argument("A mesh without indices needs three vertices for each triangle.");
        }

        return;
    }

    if (indices.size() % 3 != 0) {
        throw std::invalid_argument("The indices of a mesh need three vertices for each triangle.");
    }

    const auto outside = std::ranges::find_if(indices, [vertexCount](std::uint32_t index) { return index >= vertexCount; });
    if (outside != indices.end()) {
        throw std::invalid_argument(std::format("Index {} of the mesh names vertex {}, but the vertex count of the mesh is {}.", outside - indices.begin(), *outside, vertexCount));
    }
}

math::Triangle MeshBvh::getCorners(std::uint32_t triangle) const noexcept {
    const std::size_t base = static_cast<std::size_t>(triangle) * 3;
    if (indices.empty()) {
        return {getVertex(static_cast<std::uint32_t>(base)), getVertex(static_cast<std::uint32_t>(base + 1)), getVertex(static_cast<std::uint32_t>(base + 2))};
    }

    return {getVertex(indices[base]), getVertex(indices[base + 1]), getVertex(indices[base + 2])};
}

math::Triangle MeshBvh::getTriangle(std::uint32_t triangle) const {
    if (triangle >= getTriangleCount()) {
        throw std::invalid_argument(std::format("The triangle count of the mesh is {}, so it has no triangle {}.", getTriangleCount(), triangle));
    }

    return getCorners(triangle);
}

math::Vec3 MeshBvh::getBarycentric(const math::Triangle& triangle, math::Vec3 point) noexcept {
    const math::Vec3 ab = triangle.b - triangle.a;
    const math::Vec3 ac = triangle.c - triangle.a;
    const math::Vec3 ap = point - triangle.a;
    const float abab = math::Vec3::dot(ab, ab);
    const float abac = math::Vec3::dot(ab, ac);
    const float acac = math::Vec3::dot(ac, ac);
    const float apab = math::Vec3::dot(ap, ab);
    const float apac = math::Vec3::dot(ap, ac);
    const float denominator = abab * acac - abac * abac;
    const float b = (acac * apab - abac * apac) / denominator;
    const float c = (abab * apac - abac * apab) / denominator;
    return {1.0F - b - c, b, c};
}

// The box and the triangle are apart when their shadows on one of thirteen axes do not meet: the three axes of the box, the normal of the triangle and the nine cross products of the axes of the box with the edges of the triangle.
bool MeshBvh::overlaps(const math::Triangle& triangle, const math::Box& box) noexcept {
    const math::Vec3 center = box.min * 0.5F + box.max * 0.5F;
    const math::Vec3 half = box.max * 0.5F - box.min * 0.5F;
    const std::array<math::Vec3, 3> corners{triangle.a - center, triangle.b - center, triangle.c - center};
    const std::array<math::Vec3, 3> edges{corners[1] - corners[0], corners[2] - corners[1], corners[0] - corners[2]};
    const std::array<math::Vec3, 3> axes{math::Vec3{1.0F, 0.0F, 0.0F}, math::Vec3{0.0F, 1.0F, 0.0F}, math::Vec3{0.0F, 0.0F, 1.0F}};

    // clang-format off
    const auto separates = [&](math::Vec3 axis) {
        const float first = math::Vec3::dot(corners[0], axis);
        const float second = math::Vec3::dot(corners[1], axis);
        const float third = math::Vec3::dot(corners[2], axis);
        const float reach = half.x * std::fabs(axis.x) + half.y * std::fabs(axis.y) + half.z * std::fabs(axis.z);
        return std::min({first, second, third}) > reach || std::max({first, second, third}) < -reach;
    };
    // clang-format on

    for (const math::Vec3 axis : axes) {
        if (separates(axis)) {
            return false;
        }

        for (const math::Vec3 edge : edges) {
            if (separates(math::Vec3::cross(axis, edge))) {
                return false;
            }
        }
    }

    return !separates(math::Vec3::cross(edges[0], edges[1]));
}

std::optional<MeshBvh::Hit> MeshBvh::raycast(const math::Ray3D& ray) const {
    std::optional<Hit> nearest;
    const auto settled = [&nearest](float distance) { return nearest && distance > nearest->distance; };
    // clang-format off
    tree.visitRay(ray, settled, [&](std::uint32_t first, std::uint32_t count) {
        for (std::uint32_t position = first; position < first + count; ++position) {
            const std::uint32_t triangle = order[position];
            const math::Triangle corners = getCorners(triangle);
            const std::optional<math::RayHit3D> hit = math::Geometry3D::raycast(ray, corners);
            if (!hit) {
                continue;
            }

            const bool closer = !nearest || hit->distance < nearest->distance || (hit->distance == nearest->distance && triangle < nearest->triangle);
            if (closer) {
                const bool front = math::Vec3::dot(math::Vec3::cross(corners.b - corners.a, corners.c - corners.a), ray.direction) < 0.0F;
                nearest = Hit{.triangle = triangle, .point = hit->point, .normal = hit->normal, .barycentric = getBarycentric(corners, hit->point), .distance = hit->distance, .front = front};
            }
        }
    });
    // clang-format on
    return nearest;
}

bool MeshBvh::intersects(const math::Ray3D& ray) const {
    bool found = false;
    const auto settled = [&found](float) { return found; };
    // clang-format off
    tree.visitRay(ray, settled, [&](std::uint32_t first, std::uint32_t count) {
        for (std::uint32_t position = first; position < first + count && !found; ++position) {
            found = math::Geometry3D::raycast(ray, getCorners(order[position])).has_value();
        }
    });
    // clang-format on
    return found;
}

std::optional<MeshBvh::ClosestPoint> MeshBvh::closestPoint(math::Vec3 point, float maxDistance) const {
    EntryBounds::requirePoint(point);
    EntryBounds::requireRadius(maxDistance);
    std::optional<ClosestPoint> closest;
    const auto beyond = [&](float distance) { return distance > maxDistance || (closest && distance > closest->distance); };
    // clang-format off
    tree.visitNearest(point, beyond, [&](std::uint32_t first, std::uint32_t count) {
        for (std::uint32_t position = first; position < first + count; ++position) {
            const std::uint32_t triangle = order[position];
            const math::Vec3 onSurface = getCorners(triangle).getClosestPoint(point);
            const float distance = math::Vec3::distance(onSurface, point);
            const bool closer = !closest || distance < closest->distance || (distance == closest->distance && triangle < closest->triangle);
            if (distance <= maxDistance && closer) {
                closest = ClosestPoint{.triangle = triangle, .point = onSurface, .distance = distance};
            }
        }
    });
    // clang-format on
    return closest;
}

void MeshBvh::queryBox(const math::Box& area, std::vector<std::uint32_t>& triangles) const {
    EntryBounds::requireValid(area);
    triangles.clear();
    const auto reaches = [&area](const math::Box& box) { return box.intersects(area); };
    // clang-format off
    tree.visit(reaches, [&](std::uint32_t first, std::uint32_t count) {
        for (std::uint32_t position = first; position < first + count; ++position) {
            if (overlaps(getCorners(order[position]), area)) {
                triangles.push_back(order[position]);
            }
        }
    });
    // clang-format on
    std::ranges::sort(triangles);
}

void MeshBvh::querySphere(const math::Sphere& sphere, std::vector<std::uint32_t>& triangles) const {
    EntryBounds::requireSphere(sphere);
    triangles.clear();
    const auto reaches = [&sphere](const math::Box& box) { return EntryBounds::touches(box, sphere); };
    // clang-format off
    tree.visit(reaches, [&](std::uint32_t first, std::uint32_t count) {
        for (std::uint32_t position = first; position < first + count; ++position) {
            if (sphere.contains(getCorners(order[position]).getClosestPoint(sphere.center))) {
                triangles.push_back(order[position]);
            }
        }
    });
    // clang-format on
    std::ranges::sort(triangles);
}

} // namespace haylen::spatial3d
