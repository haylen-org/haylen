#include "haylen/math/Transform3D.hpp"

#include <cmath>
#include <stdexcept>

namespace haylen::math {

Transform3D Transform3D::fromMatrix(const Mat4& matrix) {
    const Mat4::Decomposition parts = matrix.decompose();
    return {parts.translation, parts.rotation, parts.scale};
}

Transform3D Transform3D::operator*(const Transform3D& child) const noexcept {
    return {apply(child.position), rotation * child.rotation, scale * child.scale};
}

void Transform3D::checkScale() const {
    if (scale.x == 0.0F || scale.y == 0.0F || scale.z == 0.0F || !scale.isFinite()) {
        throw std::invalid_argument("The transform scales an axis to zero or to infinity, so nothing undoes it.");
    }
}

Vec3 Transform3D::applyNormal(Vec3 normal) const {
    checkScale();
    return rotation.rotate(normal / scale).getNormalized();
}

Vec3 Transform3D::applyInverse(Vec3 point) const {
    return applyInverseVector(point - position);
}

Vec3 Transform3D::applyInverseVector(Vec3 vector) const {
    checkScale();
    const Quat back{-rotation.x, -rotation.y, -rotation.z, rotation.w};
    return back.rotate(vector) / scale;
}

Transform3D Transform3D::getInverse() const {
    checkScale();
    if (scale.x != scale.y || scale.y != scale.z) {
        throw std::invalid_argument("A transform whose scale differs between axes has no inverse of the same form. Invert its matrix instead.");
    }

    const Quat back{-rotation.x, -rotation.y, -rotation.z, rotation.w};
    const Vec3 inverseScale = Vec3{1.0F, 1.0F, 1.0F} / scale;
    return {back.rotate(-position) * inverseScale, back, inverseScale};
}

Transform3D Transform3D::lerp(const Transform3D& from, const Transform3D& to, float t) noexcept {
    return {Vec3::lerp(from.position, to.position, t), Quat::slerp(from.rotation, to.rotation, t), Vec3::lerp(from.scale, to.scale, t)};
}

bool Transform3D::approximately(const Transform3D& lhs, const Transform3D& rhs, float epsilon) noexcept {
    return Vec3::approximately(lhs.position, rhs.position, epsilon) && Quat::approximately(lhs.rotation, rhs.rotation, epsilon) && Vec3::approximately(lhs.scale, rhs.scale, epsilon);
}

} // namespace haylen::math
