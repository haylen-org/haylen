#include "haylen/math/Quat.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/math/Math.hpp"

namespace haylen::math {

const std::array<std::pair<std::string_view, Quat::EulerOrder>, 6> Quat::kEulerOrderNames{{{"xyz", EulerOrder::Xyz}, {"xzy", EulerOrder::Xzy}, {"yxz", EulerOrder::Yxz}, {"yzx", EulerOrder::Yzx}, {"zxy", EulerOrder::Zxy}, {"zyx", EulerOrder::Zyx}}};

std::optional<Quat::EulerOrder> Quat::eulerOrderFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kEulerOrderNames, name, &std::pair<std::string_view, EulerOrder>::first);
    return found != kEulerOrderNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Quat::eulerOrderName(EulerOrder value) noexcept {
    return std::ranges::find(kEulerOrderNames, value, &std::pair<std::string_view, EulerOrder>::second)->first;
}

Quat Quat::fromAxisAngle(Vec3 axis, float angle) {
    const Vec3 direction = axis.getNormalized() * std::sin(angle * 0.5F);
    return {direction.x, direction.y, direction.z, std::cos(angle * 0.5F)};
}

Quat Quat::fromAxis(char axis, float angle) noexcept {
    const float sine = std::sin(angle * 0.5F);
    const float cosine = std::cos(angle * 0.5F);
    if (axis == 'x') {
        return {sine, 0.0F, 0.0F, cosine};
    }

    if (axis == 'y') {
        return {0.0F, sine, 0.0F, cosine};
    }

    return {0.0F, 0.0F, sine, cosine};
}

// The name of the order spells the axes from the outer turn to the inner one, so the product follows it from left to right.
Quat Quat::fromEuler(Vec3 angles, EulerOrder order) {
    const std::string_view axes = eulerOrderName(order);
    Quat rotation;
    for (const char axis : axes) {
        const float angle = axis == 'x' ? angles.x : (axis == 'y' ? angles.y : angles.z);
        rotation = rotation * fromAxis(axis, angle);
    }

    return rotation.getNormalized();
}

// Starts from the largest of the trace and the diagonal, so the square root never takes a number near zero.
Quat Quat::fromMatrix(const Mat3& rotation) noexcept {
    const float m00 = rotation.get(0, 0);
    const float m11 = rotation.get(1, 1);
    const float m22 = rotation.get(2, 2);
    const float trace = m00 + m11 + m22;

    Quat result;
    if (trace > 0.0F) {
        const float scale = 0.5F / std::sqrt(trace + 1.0F);
        result = {(rotation.get(2, 1) - rotation.get(1, 2)) * scale, (rotation.get(0, 2) - rotation.get(2, 0)) * scale, (rotation.get(1, 0) - rotation.get(0, 1)) * scale, 0.25F / scale};
    } else if (m00 > m11 && m00 > m22) {
        const float scale = 2.0F * std::sqrt(1.0F + m00 - m11 - m22);
        result = {0.25F * scale, (rotation.get(0, 1) + rotation.get(1, 0)) / scale, (rotation.get(0, 2) + rotation.get(2, 0)) / scale, (rotation.get(2, 1) - rotation.get(1, 2)) / scale};
    } else if (m11 > m22) {
        const float scale = 2.0F * std::sqrt(1.0F + m11 - m00 - m22);
        result = {(rotation.get(0, 1) + rotation.get(1, 0)) / scale, 0.25F * scale, (rotation.get(1, 2) + rotation.get(2, 1)) / scale, (rotation.get(0, 2) - rotation.get(2, 0)) / scale};
    } else {
        const float scale = 2.0F * std::sqrt(1.0F + m22 - m00 - m11);
        result = {(rotation.get(0, 2) + rotation.get(2, 0)) / scale, (rotation.get(1, 2) + rotation.get(2, 1)) / scale, 0.25F * scale, (rotation.get(1, 0) - rotation.get(0, 1)) / scale};
    }

    const float length = result.getLength();
    return {result.x / length, result.y / length, result.z / length, result.w / length};
}

Quat Quat::between(Vec3 from, Vec3 to) {
    const Vec3 start = from.getNormalized();
    const Vec3 end = to.getNormalized();
    const float cosine = Vec3::dot(start, end);

    // Opposite directions have no single shortest arc, so the rotation turns half a turn around any axis at a right angle to the start.
    if (cosine < -1.0F + 1e-6F) {
        const Vec3 helper = std::fabs(start.x) < 0.9F ? Vec3::right() : Vec3::up();
        const Vec3 axis = Vec3::cross(start, helper).getNormalized();
        return {axis.x, axis.y, axis.z, 0.0F};
    }

    const Vec3 axis = Vec3::cross(start, end);
    return Quat{axis.x, axis.y, axis.z, 1.0F + cosine}.getNormalized();
}

Quat Quat::lookRotation(Vec3 direction, Vec3 up) {
    if (direction.isZero() || !direction.isFinite()) {
        throw std::invalid_argument("A rotation that looks along a direction needs a direction with a finite length above zero.");
    }

    const Vec3 forward = direction.getNormalized();
    const Vec3 side = Vec3::cross(forward, up);
    if (side.getLengthSquared() <= 1e-12F * up.getLengthSquared() || side.isZero() || !side.isFinite()) {
        throw std::invalid_argument("The up direction needs a finite length above zero and must not run along the direction to look along, or it cannot choose how the view turns around that direction.");
    }

    const Vec3 right = side.getNormalized();
    const Vec3 trueUp = Vec3::cross(right, forward);
    return fromMatrix(Mat3::fromColumns(right, trueUp, -forward));
}

Quat Quat::getNormalized() const {
    const float length = getLength();
    if (length == 0.0F || !std::isfinite(length)) {
        throw std::invalid_argument("The quaternion holds no rotation, because its length is zero or not a finite number.");
    }

    return {x / length, y / length, z / length, w / length};
}

Quat Quat::getInverse() const {
    const float squared = getLengthSquared();
    if (squared == 0.0F || !std::isfinite(squared)) {
        throw std::invalid_argument("The quaternion has no inverse, because its length is zero or not a finite number.");
    }

    return {-x / squared, -y / squared, -z / squared, w / squared};
}

float Quat::getAngle() const noexcept {
    return 2.0F * std::atan2(Vec3{x, y, z}.getLength(), std::fabs(w));
}

Vec3 Quat::getAxis() const {
    const Vec3 axis{x, y, z};
    if (axis.isZero()) {
        throw std::invalid_argument("The rotation does not turn, so it has no axis.");
    }

    return (w < 0.0F ? -axis : axis).getNormalized();
}

// Reads the angles from the matrix of the rotation. Near the poles of the middle turn, where the outer and inner turns spin around the same axis, the inner angle is zero and the outer one takes the whole turn.
Vec3 Quat::toEuler(EulerOrder order) const noexcept {
    const Mat3 matrix = toMatrix();
    const auto element = [&matrix](std::size_t row, std::size_t column) { return matrix.get(row - 1, column - 1); };
    constexpr float kPole = 0.999999F;

    switch (order) {
    case EulerOrder::Xyz: {
        const float middle = std::clamp(element(1, 3), -1.0F, 1.0F);
        if (std::fabs(middle) < kPole) {
            return {std::atan2(-element(2, 3), element(3, 3)), std::asin(middle), std::atan2(-element(1, 2), element(1, 1))};
        }

        return {std::atan2(element(3, 2), element(2, 2)), std::asin(middle), 0.0F};
    }
    case EulerOrder::Yxz: {
        const float middle = std::clamp(element(2, 3), -1.0F, 1.0F);
        if (std::fabs(middle) < kPole) {
            return {std::asin(-middle), std::atan2(element(1, 3), element(3, 3)), std::atan2(element(2, 1), element(2, 2))};
        }

        return {std::asin(-middle), std::atan2(-element(3, 1), element(1, 1)), 0.0F};
    }
    case EulerOrder::Zxy: {
        const float middle = std::clamp(element(3, 2), -1.0F, 1.0F);
        if (std::fabs(middle) < kPole) {
            return {std::asin(middle), std::atan2(-element(3, 1), element(3, 3)), std::atan2(-element(1, 2), element(2, 2))};
        }

        return {std::asin(middle), 0.0F, std::atan2(element(2, 1), element(1, 1))};
    }
    case EulerOrder::Zyx: {
        const float middle = std::clamp(element(3, 1), -1.0F, 1.0F);
        if (std::fabs(middle) < kPole) {
            return {std::atan2(element(3, 2), element(3, 3)), std::asin(-middle), std::atan2(element(2, 1), element(1, 1))};
        }

        return {0.0F, std::asin(-middle), std::atan2(-element(1, 2), element(2, 2))};
    }
    case EulerOrder::Yzx: {
        const float middle = std::clamp(element(2, 1), -1.0F, 1.0F);
        if (std::fabs(middle) < kPole) {
            return {std::atan2(-element(2, 3), element(2, 2)), std::atan2(-element(3, 1), element(1, 1)), std::asin(middle)};
        }

        return {0.0F, std::atan2(element(1, 3), element(3, 3)), std::asin(middle)};
    }
    case EulerOrder::Xzy: {
        const float middle = std::clamp(element(1, 2), -1.0F, 1.0F);
        if (std::fabs(middle) < kPole) {
            return {std::atan2(element(3, 2), element(2, 2)), std::atan2(element(1, 3), element(1, 1)), std::asin(-middle)};
        }

        return {std::atan2(-element(2, 3), element(3, 3)), 0.0F, std::asin(-middle)};
    }
    }

    return {};
}

Mat3 Quat::toMatrix() const noexcept {
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;
    return Mat3::fromColumns({1.0F - 2.0F * (yy + zz), 2.0F * (xy + wz), 2.0F * (xz - wy)}, {2.0F * (xy - wz), 1.0F - 2.0F * (xx + zz), 2.0F * (yz + wx)}, {2.0F * (xz + wy), 2.0F * (yz - wx), 1.0F - 2.0F * (xx + yy)});
}

// The twist keeps the part of the axis of rotation that runs along `axis`. A rotation of half a turn around an axis at a right angle to `axis` has no such part, so its twist is the identity.
Quat::SwingTwist Quat::getSwingTwist(Vec3 axis) const {
    const Vec3 direction = axis.getNormalized();
    const Vec3 along = direction * Vec3::dot(Vec3{x, y, z}, direction);
    const Quat raw{along.x, along.y, along.z, w};
    const Quat twist = raw.getLengthSquared() == 0.0F ? identity() : raw.getNormalized();
    const Quat swing = (*this * twist.getInverse()).getNormalized();
    return {swing, twist};
}

float Quat::angleBetween(const Quat& from, const Quat& to) noexcept {
    const Quat conjugate{-from.x, -from.y, -from.z, from.w};
    return (conjugate * to).getAngle();
}

Quat Quat::getShortestTo(const Quat& other) const noexcept {
    if (dot(*this, other) < 0.0F) {
        return {-other.x, -other.y, -other.z, -other.w};
    }

    return other;
}

Quat Quat::slerp(const Quat& from, const Quat& to, float t) noexcept {
    const Quat end = from.getShortestTo(to);
    if (t == 0.0F) {
        return from;
    }

    if (t == 1.0F) {
        return end;
    }

    // Close rotations divide by a sine near zero, where the straight line between them is as exact and safe.
    const float cosine = std::min(dot(from, end), 1.0F);
    if (cosine > 0.9995F) {
        return nlerp(from, end, t);
    }

    const float angle = std::acos(cosine);
    const float sine = std::sin(angle);
    const float fromWeight = std::sin((1.0F - t) * angle) / sine;
    const float toWeight = std::sin(t * angle) / sine;
    const Quat blended{from.x * fromWeight + end.x * toWeight, from.y * fromWeight + end.y * toWeight, from.z * fromWeight + end.z * toWeight, from.w * fromWeight + end.w * toWeight};
    const float length = blended.getLength();
    return {blended.x / length, blended.y / length, blended.z / length, blended.w / length};
}

Quat Quat::nlerp(const Quat& from, const Quat& to, float t) noexcept {
    const Quat end = from.getShortestTo(to);
    const Quat blended{Math::lerp(from.x, end.x, t), Math::lerp(from.y, end.y, t), Math::lerp(from.z, end.z, t), Math::lerp(from.w, end.w, t)};
    const float length = blended.getLength();
    return {blended.x / length, blended.y / length, blended.z / length, blended.w / length};
}

bool Quat::approximately(const Quat& lhs, const Quat& rhs, float epsilon) noexcept {
    const Quat aligned = lhs.getShortestTo(rhs);
    return Math::approximately(lhs.x, aligned.x, epsilon) && Math::approximately(lhs.y, aligned.y, epsilon) && Math::approximately(lhs.z, aligned.z, epsilon) && Math::approximately(lhs.w, aligned.w, epsilon);
}

} // namespace haylen::math
