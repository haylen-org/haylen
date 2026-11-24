#include "haylen/math/Triangle3.hpp"

#include <stdexcept>

namespace haylen::math {

Vec3 Triangle3::getNormal() const {
    const Vec3 perpendicular = Vec3::cross(b - a, c - a);
    if (perpendicular.isZero() || !perpendicular.isFinite()) {
        throw std::invalid_argument("The triangle has no area, so it has no normal.");
    }

    return perpendicular.getNormalized();
}

// Finds the region of the point from the corners outward, the corners first, then the edges, then the face, using the barycentric coordinates that the dot products give.
Vec3 Triangle3::getClosestPoint(Vec3 point) const noexcept {
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const Vec3 ap = point - a;
    const float d1 = Vec3::dot(ab, ap);
    const float d2 = Vec3::dot(ac, ap);
    if (d1 <= 0.0F && d2 <= 0.0F) {
        return a;
    }

    const Vec3 bp = point - b;
    const float d3 = Vec3::dot(ab, bp);
    const float d4 = Vec3::dot(ac, bp);
    if (d3 >= 0.0F && d4 <= d3) {
        return b;
    }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) {
        return a + ab * (d1 / (d1 - d3));
    }

    const Vec3 cp = point - c;
    const float d5 = Vec3::dot(ab, cp);
    const float d6 = Vec3::dot(ac, cp);
    if (d6 >= 0.0F && d5 <= d6) {
        return c;
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) {
        return a + ac * (d2 / (d2 - d6));
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0F && (d4 - d3) >= 0.0F && (d5 - d6) >= 0.0F) {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }

    const float denominator = 1.0F / (va + vb + vc);
    return a + ab * (vb * denominator) + ac * (vc * denominator);
}

} // namespace haylen::math
