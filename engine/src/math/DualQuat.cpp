#include "haylen/math/DualQuat.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/math/Math.hpp"

namespace haylen::math {

DualQuat DualQuat::fromRotationTranslation(const Quat& rotation, Vec3 translation) noexcept {
    const Quat moved = Quat{translation.x, translation.y, translation.z, 0.0F} * rotation;
    return {rotation, {moved.x * 0.5F, moved.y * 0.5F, moved.z * 0.5F, moved.w * 0.5F}};
}

DualQuat DualQuat::blend(std::span<const DualQuat> parts, std::span<const float> weights) {
    if (parts.size() != weights.size()) {
        throw std::invalid_argument("A blend of dual quaternions needs one weight for each dual quaternion.");
    }

    if (parts.empty()) {
        throw std::invalid_argument("A blend of dual quaternions needs at least one dual quaternion.");
    }

    // Each part joins on the side of the first, so two forms of one rotation never cancel out.
    DualQuat sum{{0.0F, 0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 0.0F}};
    float total = 0.0F;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        const float weight = weights[index];
        if (!std::isfinite(weight) || weight < 0.0F) {
            throw std::invalid_argument("The weights of a blend of dual quaternions must be finite and not negative.");
        }

        const float side = Quat::dot(parts[index].real, parts.front().real) < 0.0F ? -weight : weight;
        const DualQuat& part = parts[index];
        sum.real = {sum.real.x + part.real.x * side, sum.real.y + part.real.y * side, sum.real.z + part.real.z * side, sum.real.w + part.real.w * side};
        sum.dual = {sum.dual.x + part.dual.x * side, sum.dual.y + part.dual.y * side, sum.dual.z + part.dual.z * side, sum.dual.w + part.dual.w * side};
        total += weight;
    }

    if (total == 0.0F) {
        throw std::invalid_argument("The weights of a blend of dual quaternions add up to zero, so they blend nothing.");
    }

    return sum.getNormalized();
}

Vec3 DualQuat::getTranslation() const noexcept {
    const Quat moved = dual * Quat{-real.x, -real.y, -real.z, real.w};
    return {moved.x * 2.0F, moved.y * 2.0F, moved.z * 2.0F};
}

// Dividing both parts by the length of the real part keeps the translation, which the dual part holds together with the rotation.
DualQuat DualQuat::getNormalized() const {
    const float length = real.getLength();
    if (length == 0.0F || !std::isfinite(length)) {
        throw std::invalid_argument("The dual quaternion holds no rotation, because the length of its real part is zero or not a finite number.");
    }

    return {{real.x / length, real.y / length, real.z / length, real.w / length}, {dual.x / length, dual.y / length, dual.z / length, dual.w / length}};
}

bool DualQuat::approximately(const DualQuat& lhs, const DualQuat& rhs, float epsilon) noexcept {
    return Quat::approximately(lhs.real, rhs.real, epsilon) && Vec3::approximately(lhs.getTranslation(), rhs.getTranslation(), epsilon);
}

} // namespace haylen::math
