#include "haylen/math/Frustum.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::math {

const std::array<std::pair<std::string_view, Frustum::Containment>, 3> Frustum::kContainmentNames{{{"outside", Containment::Outside}, {"intersecting", Containment::Intersecting}, {"inside", Containment::Inside}}};

std::optional<Frustum::Containment> Frustum::containmentFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kContainmentNames, name, &std::pair<std::string_view, Containment>::first);
    return found != kContainmentNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Frustum::containmentName(Containment value) noexcept {
    return std::ranges::find(kContainmentNames, value, &std::pair<std::string_view, Containment>::second)->first;
}

// A point is inside when its clip coordinates lie within `-w` and `w` across the view and within the range of depth along it, and each of those conditions is a plane made of the rows of the matrix. A projection without a far end gives a plane of depth with no normal, which holds every point, so the frustum leaves it out.
Frustum Frustum::fromMatrix(const Mat4& viewProjection, Mat4::DepthRange depthRange) {
    const Vec4 x = viewProjection.getRow(0);
    const Vec4 y = viewProjection.getRow(1);
    const Vec4 z = viewProjection.getRow(2);
    const Vec4 w = viewProjection.getRow(3);
    const Vec4 lowDepth = depthRange == Mat4::DepthRange::ZeroToOne ? z : w + z;
    const std::array<Vec4, 6> coefficients{w + x, w - x, w + y, w - y, lowDepth, w - z};

    Frustum frustum;
    for (const Vec4 plane : coefficients) {
        const Vec3 normal = plane.getXyz();
        if (normal.isZero()) {
            continue;
        }

        frustum.planes[frustum.planeCount] = Plane::fromNormal(normal, -plane.w);
        ++frustum.planeCount;
    }

    return frustum;
}

bool Frustum::contains(Vec3 point, float tolerance) const noexcept {
    for (const Plane& plane : getPlanes()) {
        if (plane.getSignedDistance(point) < -tolerance) {
            return false;
        }
    }

    return true;
}

// A shape reaches across a plane by its radius plus the extent of its box along the normal of the plane, so the test is the same for spheres and boxes.
Frustum::Containment Frustum::classify(Vec3 center, Vec3 halfExtents, float radius, float tolerance) const noexcept {
    Containment result = Containment::Inside;
    for (const Plane& plane : getPlanes()) {
        const Vec3 normal = plane.normal;
        const float reach = radius + halfExtents.x * std::fabs(normal.x) + halfExtents.y * std::fabs(normal.y) + halfExtents.z * std::fabs(normal.z);
        const float distance = plane.getSignedDistance(center);
        if (distance < -reach - tolerance) {
            return Containment::Outside;
        }

        if (distance < reach - tolerance) {
            result = Containment::Intersecting;
        }
    }

    return result;
}

Frustum::Containment Frustum::classify(const Sphere& sphere, float tolerance) const noexcept {
    return classify(sphere.center, {}, sphere.radius, tolerance);
}

Frustum::Containment Frustum::classify(const Box& box, float tolerance) const noexcept {
    return classify(box.getCenter(), box.getHalfExtents(), 0.0F, tolerance);
}

} // namespace haylen::math
