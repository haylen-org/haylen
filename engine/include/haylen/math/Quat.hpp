#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Mat3.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::math {

// A rotation as a unit quaternion with the components `(x, y, z, w)`, where `w` is the real part. Every function that returns a rotation returns it normalized.
struct Quat {
    // The order of the three turns of Euler angles. The order `Yxz` turns an object about its own y axis, then about its own x axis, then about its own z axis, which is the same as the turns about the fixed z, x and y axes in that order, so `fromEuler` of `Yxz` is `qy * qx * qz`.
    enum class EulerOrder : std::uint8_t {
        Xyz,
        Xzy,
        Yxz,
        Yzx,
        Zxy,
        Zyx,
    };

    // The parts of a rotation around an axis: `twist` turns around the axis and `swing` turns the axis itself, with `swing * twist` giving the rotation.
    struct SwingTwist;

    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;

    [[nodiscard]] static constexpr Quat identity() noexcept {
        return {};
    }

    // Throws `std::invalid_argument` for an axis without a direction.
    [[nodiscard]] static Quat fromAxisAngle(Vec3 axis, float angle);
    [[nodiscard]] static Quat fromEuler(Vec3 angles, EulerOrder order);

    // Returns the rotation of a matrix whose columns have a length of one and stand at right angles, and whose determinant is one.
    [[nodiscard]] static Quat fromMatrix(const Mat3& rotation) noexcept;

    // Returns the shortest rotation that turns the direction `from` onto the direction `to`, which turns half a turn around an axis at a right angle to `from` when they point opposite ways. Throws `std::invalid_argument` when either has no direction.
    [[nodiscard]] static Quat between(Vec3 from, Vec3 to);

    // Returns the rotation that turns `Vec3::forward()` onto `direction` and keeps the up axis as close to `up` as it can, like a camera that looks along `direction`. Throws `std::invalid_argument` when `direction` has no direction or runs along `up`.
    [[nodiscard]] static Quat lookRotation(Vec3 direction, Vec3 up = Vec3::up());

    [[nodiscard]] static std::optional<EulerOrder> eulerOrderFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view eulerOrderName(EulerOrder value) noexcept;

    [[nodiscard]] constexpr bool operator==(const Quat&) const noexcept = default;

    // Returns the rotation that turns by `rhs` first and by this rotation after it.
    [[nodiscard]] constexpr Quat operator*(const Quat& rhs) const noexcept {
        return {w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y, w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x, w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w, w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z};
    }

    [[nodiscard]] constexpr Vec3 rotate(Vec3 vector) const noexcept {
        const Vec3 axis{x, y, z};
        const Vec3 turned = Vec3::cross(axis, vector) * 2.0F;
        return vector + turned * w + Vec3::cross(axis, turned);
    }

    [[nodiscard]] constexpr float getLengthSquared() const noexcept {
        return x * x + y * y + z * z + w * w;
    }
    [[nodiscard]] float getLength() const noexcept {
        return std::sqrt(getLengthSquared());
    }

    // Throws `std::invalid_argument` for a quaternion of length zero or with a component that is not finite, which holds no rotation.
    [[nodiscard]] Quat getNormalized() const;

    // Throws `std::invalid_argument` for a quaternion of length zero, which has no inverse.
    [[nodiscard]] Quat getInverse() const;

    // Returns the angle of the rotation, from 0 to pi radians.
    [[nodiscard]] float getAngle() const noexcept;

    // Returns the axis that the rotation turns around by `getAngle()`. Throws `std::invalid_argument` for a rotation that does not turn, which has no axis.
    [[nodiscard]] Vec3 getAxis() const;

    [[nodiscard]] Vec3 toEuler(EulerOrder order) const noexcept;
    [[nodiscard]] Mat3 toMatrix() const noexcept;

    // Splits the rotation into a twist around `axis` and the swing that turns the axis, such as the turn of a forearm around its bone and the bend of the elbow. Throws `std::invalid_argument` for an axis without a direction.
    [[nodiscard]] SwingTwist getSwingTwist(Vec3 axis) const;

    [[nodiscard]] static constexpr float dot(const Quat& lhs, const Quat& rhs) noexcept {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
    }

    // Returns the angle of the rotation that turns `from` onto `to`, from 0 to pi radians, for two rotations of length one.
    [[nodiscard]] static float angleBetween(const Quat& from, const Quat& to) noexcept;

    // Interpolates along the shorter arc at a constant speed. Returns `from` at 0 and `to`, or its opposite form on the shorter arc, at 1.
    [[nodiscard]] static Quat slerp(const Quat& from, const Quat& to, float t) noexcept;

    // Interpolates along the shorter arc faster than `slerp`, at a speed that varies a little along the arc.
    [[nodiscard]] static Quat nlerp(const Quat& from, const Quat& to, float t) noexcept;

    // Compares two rotations with the tolerance of `Math::approximately`, so a quaternion and its opposite, which hold the same rotation, compare equal.
    [[nodiscard]] static bool approximately(const Quat& lhs, const Quat& rhs, float epsilon = 1e-5F) noexcept;

  private:
    static const std::array<std::pair<std::string_view, EulerOrder>, 6> kEulerOrderNames;

    [[nodiscard]] static Quat fromAxis(char axis, float angle) noexcept;
    [[nodiscard]] Quat getShortestTo(const Quat& other) const noexcept;
};

struct Quat::SwingTwist {
    Quat swing;
    Quat twist;
};

} // namespace haylen::math
