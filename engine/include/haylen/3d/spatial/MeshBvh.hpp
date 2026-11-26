#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "haylen/3d/spatial/BvhNodes.hpp"
#include "haylen/math/Box.hpp"
#include "haylen/math/Ray3D.hpp"
#include "haylen/math/Sphere.hpp"
#include "haylen/math/Triangle.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::spatial3d {

// A bounding volume hierarchy over the triangles of a mesh, which casts rays against the exact surface, such as the shot of a weapon at a character or the pointer at a model, and finds the closest point of the surface and the triangles near a shape. A ray never slips between two triangles that share an edge. Triangles are counted from zero in the order of the mesh.
class MeshBvh final {
  public:
    // Where a ray meets the surface: the triangle, the point, the normal of the side the ray hits, which faces the ray, whether that side is the front, where the corners run counter-clockwise, the weights of the three corners at the point and the distance from the origin of the ray.
    struct Hit {
        std::uint32_t triangle = 0;
        math::Vec3 point;
        math::Vec3 normal;
        math::Vec3 barycentric;
        float distance = 0.0F;
        bool front = true;
    };

    struct ClosestPoint {
        std::uint32_t triangle = 0;
        math::Vec3 point;
        float distance = 0.0F;
    };

    // Takes the positions as three numbers for each vertex and the indices as three vertices for each triangle, or no indices for a mesh whose positions list every triangle in turn, and keeps both without a copy. Throws `std::invalid_argument` for a position that is not finite, an index past the last vertex or counts that do not make whole vertices and triangles.
    explicit MeshBvh(std::vector<float> vertexPositions, std::vector<std::uint32_t> triangleIndices = {});

    [[nodiscard]] std::size_t getVertexCount() const noexcept {
        return positions.size() / 3;
    }
    [[nodiscard]] std::size_t getTriangleCount() const noexcept {
        return order.size();
    }
    [[nodiscard]] std::size_t getNodeCount() const noexcept {
        return tree.size();
    }
    [[nodiscard]] int getDepth() const noexcept {
        return tree.getDepth();
    }

    // The positions and indices the tree was made with, which other parts of the engine build their own data from, such as the collision shapes of 3D physics.
    [[nodiscard]] const std::vector<float>& getPositions() const noexcept {
        return positions;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& getIndices() const noexcept {
        return indices;
    }

    // Returns the box that holds every triangle, or nothing for a mesh without triangles.
    [[nodiscard]] std::optional<math::Box> getBounds() const noexcept {
        return tree.getBounds();
    }

    // Throws `std::invalid_argument` for a triangle past the last one.
    [[nodiscard]] math::Triangle getTriangle(std::uint32_t triangle) const;

    // Returns the nearest hit of the ray on the surface, and of hits at the same distance the one on the triangle that comes first.
    [[nodiscard]] std::optional<Hit> raycast(const math::Ray3D& ray) const;

    // Tells whether the ray hits the surface anywhere, which stops at the first hit, such as for a line of sight.
    [[nodiscard]] bool intersects(const math::Ray3D& ray) const;

    // Returns the point of the surface closest to `point` within `maxDistance`, and of points at the same distance the one on the triangle that comes first.
    [[nodiscard]] std::optional<ClosestPoint> closestPoint(math::Vec3 point, float maxDistance = std::numeric_limits<float>::infinity()) const;

    // Fills `triangles` with the triangles that touch the shape, in ascending order.
    void queryBox(const math::Box& area, std::vector<std::uint32_t>& triangles) const;
    void querySphere(const math::Sphere& sphere, std::vector<std::uint32_t>& triangles) const;

  private:
    [[nodiscard]] math::Vec3 getVertex(std::uint32_t vertex) const noexcept {
        const std::size_t base = static_cast<std::size_t>(vertex) * 3;
        return {positions[base], positions[base + 1], positions[base + 2]};
    }
    [[nodiscard]] math::Triangle getCorners(std::uint32_t triangle) const noexcept;

    // Tells whether a triangle and a box touch, by the separating axes of the two shapes.
    [[nodiscard]] static bool overlaps(const math::Triangle& triangle, const math::Box& box) noexcept;
    [[nodiscard]] static math::Vec3 getBarycentric(const math::Triangle& triangle, math::Vec3 point) noexcept;

    void validate() const;

    std::vector<float> positions;
    std::vector<std::uint32_t> indices;

    // The triangles in the order of the leaves of the tree.
    std::vector<std::uint32_t> order;
    BvhNodes tree;
};

} // namespace haylen::spatial3d
