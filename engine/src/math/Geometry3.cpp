#include "haylen/math/Geometry3.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "haylen/math/Mat3.hpp"

namespace haylen::math {

// Turns the ray into a space where it runs along z and tests on which side of each edge it passes, which gives two triangles that share an edge the same answer for a ray through that edge. Edge tests that land on exactly zero are redone in double precision, where rounding cannot flip their sign.
std::optional<RayHit3> Geometry3::raycast(const Ray3& ray, const Triangle3& triangle) noexcept {
    const std::array<float, 3> direction{ray.direction.x, ray.direction.y, ray.direction.z};
    std::size_t kz = 0;
    for (std::size_t axis = 1; axis < 3; ++axis) {
        if (std::fabs(direction[axis]) > std::fabs(direction[kz])) {
            kz = axis;
        }
    }

    std::size_t kx = (kz + 1) % 3;
    std::size_t ky = (kx + 1) % 3;
    if (direction[kz] < 0.0F) {
        std::swap(kx, ky);
    }

    const float shearX = direction[kx] / direction[kz];
    const float shearY = direction[ky] / direction[kz];
    const float shearZ = 1.0F / direction[kz];

    const Vec3 relativeA = triangle.a - ray.origin;
    const Vec3 relativeB = triangle.b - ray.origin;
    const Vec3 relativeC = triangle.c - ray.origin;
    const std::array<float, 3> a{relativeA.x, relativeA.y, relativeA.z};
    const std::array<float, 3> b{relativeB.x, relativeB.y, relativeB.z};
    const std::array<float, 3> c{relativeC.x, relativeC.y, relativeC.z};
    const float ax = a[kx] - shearX * a[kz];
    const float ay = a[ky] - shearY * a[kz];
    const float bx = b[kx] - shearX * b[kz];
    const float by = b[ky] - shearY * b[kz];
    const float cx = c[kx] - shearX * c[kz];
    const float cy = c[ky] - shearY * c[kz];

    float u = cx * by - cy * bx;
    float v = ax * cy - ay * cx;
    float w = bx * ay - by * ax;
    if (u == 0.0F || v == 0.0F || w == 0.0F) {
        u = static_cast<float>(static_cast<double>(cx) * static_cast<double>(by) - static_cast<double>(cy) * static_cast<double>(bx));
        v = static_cast<float>(static_cast<double>(ax) * static_cast<double>(cy) - static_cast<double>(ay) * static_cast<double>(cx));
        w = static_cast<float>(static_cast<double>(bx) * static_cast<double>(ay) - static_cast<double>(by) * static_cast<double>(ax));
    }

    if ((u < 0.0F || v < 0.0F || w < 0.0F) && (u > 0.0F || v > 0.0F || w > 0.0F)) {
        return std::nullopt;
    }

    const float determinant = u + v + w;
    if (determinant == 0.0F) {
        return std::nullopt;
    }

    const float scaled = u * shearZ * a[kz] + v * shearZ * b[kz] + w * shearZ * c[kz];
    const float distance = scaled / determinant;
    if (!isWithin(ray, distance)) {
        return std::nullopt;
    }

    const Vec3 normal = Vec3::cross(triangle.b - triangle.a, triangle.c - triangle.a).getNormalized();
    return RayHit3{distance, ray.at(distance), Vec3::dot(normal, ray.direction) > 0.0F ? -normal : normal};
}

// Cuts the ray by the pair of planes of each axis and keeps the part inside all three, whose start is the hit.
std::optional<RayHit3> Geometry3::raycast(const Ray3& ray, const Box3& box) noexcept {
    if (box.contains(ray.origin)) {
        return RayHit3{0.0F, ray.origin, {}};
    }

    const std::array<float, 3> origin{ray.origin.x, ray.origin.y, ray.origin.z};
    const std::array<float, 3> direction{ray.direction.x, ray.direction.y, ray.direction.z};
    const std::array<float, 3> low{box.min.x, box.min.y, box.min.z};
    const std::array<float, 3> high{box.max.x, box.max.y, box.max.z};

    float enter = -std::numeric_limits<float>::infinity();
    float leave = std::numeric_limits<float>::infinity();
    std::size_t enterAxis = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (std::fabs(direction[axis]) < kParallel) {
            if (origin[axis] < low[axis] || origin[axis] > high[axis]) {
                return std::nullopt;
            }

            continue;
        }

        const float toLow = (low[axis] - origin[axis]) / direction[axis];
        const float toHigh = (high[axis] - origin[axis]) / direction[axis];
        const float first = std::min(toLow, toHigh);
        const float last = std::max(toLow, toHigh);
        if (first > enter) {
            enter = first;
            enterAxis = axis;
        }

        leave = std::min(leave, last);
    }

    if (enter > leave || !isWithin(ray, enter)) {
        return std::nullopt;
    }

    std::array<float, 3> normal{};
    normal[enterAxis] = direction[enterAxis] > 0.0F ? -1.0F : 1.0F;
    return RayHit3{enter, ray.at(enter), {normal[0], normal[1], normal[2]}};
}

// Casts in the space of the box, where it is a box along the axes, and turns the hit back.
std::optional<RayHit3> Geometry3::raycast(const Ray3& ray, const Obb& box) noexcept {
    const Quat back{-box.rotation.x, -box.rotation.y, -box.rotation.z, box.rotation.w};
    const Ray3 local{back.rotate(ray.origin - box.center), back.rotate(ray.direction), ray.length};
    const std::optional<RayHit3> hit = raycast(local, Box3{-box.halfExtents, box.halfExtents});
    if (!hit) {
        return std::nullopt;
    }

    return RayHit3{hit->distance, ray.at(hit->distance), box.rotation.rotate(hit->normal)};
}

std::optional<RayHit3> Geometry3::raycastSphere(const Ray3& ray, Vec3 center, float radius) noexcept {
    const Vec3 offset = ray.origin - center;
    const float along = Vec3::dot(offset, ray.direction);
    const float outside = offset.getLengthSquared() - radius * radius;
    if (outside <= 0.0F) {
        return RayHit3{0.0F, ray.origin, {}};
    }

    const float discriminant = along * along - outside;
    if (discriminant < 0.0F || along > 0.0F) {
        return std::nullopt;
    }

    const float distance = -along - std::sqrt(discriminant);
    if (!isWithin(ray, distance)) {
        return std::nullopt;
    }

    const Vec3 point = ray.at(distance);
    return RayHit3{distance, point, (point - center) / radius};
}

std::optional<RayHit3> Geometry3::raycast(const Ray3& ray, const Sphere& sphere) noexcept {
    return raycastSphere(ray, sphere.center, sphere.radius);
}

std::optional<RayHit3> Geometry3::raycast(const Ray3& ray, const Plane& plane) noexcept {
    const float facing = Vec3::dot(plane.normal, ray.direction);
    if (std::fabs(facing) < kParallel) {
        return std::nullopt;
    }

    const float distance = -plane.getSignedDistance(ray.origin) / facing;
    if (!isWithin(ray, distance)) {
        return std::nullopt;
    }

    return RayHit3{distance, ray.at(distance), facing < 0.0F ? plane.normal : -plane.normal};
}

// Takes the nearest of the hits on the side of the capsule, a cylinder around its axis, and on the spheres at its ends.
std::optional<RayHit3> Geometry3::raycast(const Ray3& ray, const Capsule3& capsule) noexcept {
    if (capsule.contains(ray.origin)) {
        return RayHit3{0.0F, ray.origin, {}};
    }

    std::optional<RayHit3> nearest = raycastSphere(ray, capsule.start, capsule.radius);
    const std::optional<RayHit3> endHit = raycastSphere(ray, capsule.end, capsule.radius);
    if (endHit && (!nearest || endHit->distance < nearest->distance)) {
        nearest = endHit;
    }

    const Vec3 axis = capsule.end - capsule.start;
    const Vec3 offset = ray.origin - capsule.start;
    const float axisSquared = axis.getLengthSquared();
    const float axisAlong = Vec3::dot(axis, ray.direction);
    const float quadratic = axisSquared - axisAlong * axisAlong;
    if (quadratic <= kParallel * axisSquared) {
        return nearest;
    }

    const float offsetAlong = Vec3::dot(axis, offset);
    const float linear = axisSquared * Vec3::dot(ray.direction, offset) - offsetAlong * axisAlong;
    const float constant = axisSquared * offset.getLengthSquared() - offsetAlong * offsetAlong - capsule.radius * capsule.radius * axisSquared;
    const float discriminant = linear * linear - quadratic * constant;
    if (discriminant < 0.0F) {
        return nearest;
    }

    const float distance = (-linear - std::sqrt(discriminant)) / quadratic;
    const float height = offsetAlong + distance * axisAlong;
    if (height <= 0.0F || height >= axisSquared || !isWithin(ray, distance) || (nearest && nearest->distance <= distance)) {
        return nearest;
    }

    const Vec3 point = ray.at(distance);
    const Vec3 onAxis = capsule.start + axis * (height / axisSquared);
    return RayHit3{distance, point, (point - onAxis) / capsule.radius};
}

// Projects both boxes on the three axes of each box and the nine cross products of their axes. A tiny amount added to the products of parallel axes keeps their cross products, which are near zero, from separating boxes by rounding.
bool Geometry3::intersects(const Obb& lhs, const Obb& rhs) noexcept {
    const Mat3 lhsAxes = lhs.getAxes();
    const Mat3 rhsAxes = rhs.getAxes();
    const std::array<float, 3> lhsExtents{lhs.halfExtents.x, lhs.halfExtents.y, lhs.halfExtents.z};
    const std::array<float, 3> rhsExtents{rhs.halfExtents.x, rhs.halfExtents.y, rhs.halfExtents.z};

    std::array<std::array<float, 3>, 3> turn{};
    std::array<std::array<float, 3>, 3> absolute{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            turn[row][column] = Vec3::dot(lhsAxes.getColumn(row), rhsAxes.getColumn(column));
            absolute[row][column] = std::fabs(turn[row][column]) + 1e-6F;
        }
    }

    const Vec3 between = rhs.center - lhs.center;
    const std::array<float, 3> t{Vec3::dot(between, lhsAxes.getColumn(0)), Vec3::dot(between, lhsAxes.getColumn(1)), Vec3::dot(between, lhsAxes.getColumn(2))};

    for (std::size_t i = 0; i < 3; ++i) {
        const float reach = lhsExtents[i] + rhsExtents[0] * absolute[i][0] + rhsExtents[1] * absolute[i][1] + rhsExtents[2] * absolute[i][2];
        if (std::fabs(t[i]) > reach) {
            return false;
        }
    }

    for (std::size_t j = 0; j < 3; ++j) {
        const float reach = lhsExtents[0] * absolute[0][j] + lhsExtents[1] * absolute[1][j] + lhsExtents[2] * absolute[2][j] + rhsExtents[j];
        const float distance = t[0] * turn[0][j] + t[1] * turn[1][j] + t[2] * turn[2][j];
        if (std::fabs(distance) > reach) {
            return false;
        }
    }

    for (std::size_t i = 0; i < 3; ++i) {
        const std::size_t i1 = (i + 1) % 3;
        const std::size_t i2 = (i + 2) % 3;
        for (std::size_t j = 0; j < 3; ++j) {
            const std::size_t j1 = (j + 1) % 3;
            const std::size_t j2 = (j + 2) % 3;
            const float lhsReach = lhsExtents[i1] * absolute[i2][j] + lhsExtents[i2] * absolute[i1][j];
            const float rhsReach = rhsExtents[j1] * absolute[i][j2] + rhsExtents[j2] * absolute[i][j1];
            const float distance = t[i2] * turn[i1][j] - t[i1] * turn[i2][j];
            if (std::fabs(distance) > lhsReach + rhsReach) {
                return false;
            }
        }
    }

    return true;
}

// Minimizes the distance between a point of each segment, clamping the parameter of each to the segment, and handles segments of zero length as points.
std::pair<Vec3, Vec3> Geometry3::closestPoints(const Segment3& lhs, const Segment3& rhs) noexcept {
    const Vec3 first = lhs.end - lhs.start;
    const Vec3 second = rhs.end - rhs.start;
    const Vec3 offset = lhs.start - rhs.start;
    const float firstSquared = first.getLengthSquared();
    const float secondSquared = second.getLengthSquared();
    const float secondOffset = Vec3::dot(second, offset);

    if (firstSquared == 0.0F && secondSquared == 0.0F) {
        return {lhs.start, rhs.start};
    }

    if (firstSquared == 0.0F) {
        return {lhs.start, rhs.getClosestPoint(lhs.start)};
    }

    const float firstOffset = Vec3::dot(first, offset);
    if (secondSquared == 0.0F) {
        return {lhs.getClosestPoint(rhs.start), rhs.start};
    }

    const float across = Vec3::dot(first, second);
    const float denominator = firstSquared * secondSquared - across * across;
    float s = denominator > 0.0F ? std::clamp((across * secondOffset - firstOffset * secondSquared) / denominator, 0.0F, 1.0F) : 0.0F;
    float t = (across * s + secondOffset) / secondSquared;
    if (t < 0.0F) {
        t = 0.0F;
        s = std::clamp(-firstOffset / firstSquared, 0.0F, 1.0F);
    } else if (t > 1.0F) {
        t = 1.0F;
        s = std::clamp((across - firstOffset) / firstSquared, 0.0F, 1.0F);
    }

    return {lhs.start + first * s, rhs.start + second * t};
}

} // namespace haylen::math
