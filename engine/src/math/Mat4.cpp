#include "haylen/math/Mat4.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>

namespace haylen::math {

const std::array<std::pair<std::string_view, Mat4::DepthRange>, 2> Mat4::kDepthRangeNames{{{"zeroToOne", DepthRange::ZeroToOne}, {"negativeOneToOne", DepthRange::NegativeOneToOne}}};

std::optional<Mat4::DepthRange> Mat4::depthRangeFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kDepthRangeNames, name, &std::pair<std::string_view, DepthRange>::first);
    return found != kDepthRangeNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Mat4::depthRangeName(DepthRange value) noexcept {
    return std::ranges::find(kDepthRangeNames, value, &std::pair<std::string_view, DepthRange>::second)->first;
}

Mat4 Mat4::compose(Vec3 translation, const Quat& rotation, Vec3 scale) noexcept {
    const Mat3 turn = rotation.toMatrix();
    const Mat3 linear = Mat3::fromColumns(turn.getColumn(0) * scale.x, turn.getColumn(1) * scale.y, turn.getColumn(2) * scale.z);
    return fromLinear(linear, translation);
}

// The view matrix is the inverse of the placement of the camera, whose rotation turns the forward axis toward the target.
Mat4 Mat4::lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 direction = target - eye;
    if (direction.isZero() || !direction.isFinite()) {
        throw std::invalid_argument("The eye and the target of a view are the same point, so the view has no direction.");
    }

    const Mat3 inverseTurn = Quat::lookRotation(direction, up).toMatrix().getTransposed();
    return fromLinear(inverseTurn, -(inverseTurn * eye));
}

std::pair<float, float> Mat4::getDepthEnds(DepthRange depthRange, bool reversedDepth) noexcept {
    const float low = depthRange == DepthRange::ZeroToOne ? 0.0F : -1.0F;
    return reversedDepth ? std::pair{1.0F, low} : std::pair{low, 1.0F};
}

void Mat4::checkDepthPlanes(float nearPlane, float farPlane, std::string_view projection) {
    if (!std::isfinite(nearPlane) || nearPlane <= 0.0F) {
        throw std::invalid_argument(std::format("The near plane of {} must be at a finite distance above 0.", projection));
    }

    if (!(farPlane > nearPlane)) {
        throw std::invalid_argument(std::format("The far plane of {} must lie beyond its near plane.", projection));
    }
}

// The edges are the tangents of the angles of the view at a distance of one. The depth of a point at the distance `d` in front of the camera is `-a + b / d`, which gives the near plane and the far plane the ends of the range of depth.
Mat4 Mat4::projectTangents(float left, float right, float bottom, float top, float nearPlane, float farPlane, DepthRange depthRange, bool reversedDepth) noexcept {
    const auto [nearDepth, farDepth] = getDepthEnds(depthRange, reversedDepth);
    float scale = 0.0F;
    float offset = 0.0F;
    if (std::isinf(farPlane)) {
        offset = (nearDepth - farDepth) * nearPlane;
        scale = -farDepth;
    } else {
        offset = (nearDepth - farDepth) * nearPlane * farPlane / (farPlane - nearPlane);
        scale = offset / nearPlane - nearDepth;
    }

    Mat4 projection;
    projection.values = {2.0F / (right - left), 0.0F, 0.0F, 0.0F, 0.0F, 2.0F / (top - bottom), 0.0F, 0.0F, (right + left) / (right - left), (top + bottom) / (top - bottom), scale, -1.0F, 0.0F, 0.0F, offset, 0.0F};
    return projection;
}

Mat4 Mat4::perspective(const Perspective& options) {
    if (!(options.fieldOfView > 0.0F && options.fieldOfView < Math::kPi)) {
        throw std::invalid_argument("The field of view of a perspective must be an angle above 0 and below pi radians.");
    }

    if (!std::isfinite(options.aspect) || options.aspect <= 0.0F) {
        throw std::invalid_argument("The aspect of a perspective must be a finite number above 0.");
    }

    checkDepthPlanes(options.nearPlane, options.farPlane, "a perspective");
    const float top = std::tan(options.fieldOfView * 0.5F);
    const float right = top * options.aspect;
    return projectTangents(-right, right, -top, top, options.nearPlane, options.farPlane, options.depthRange, options.reversedDepth);
}

Mat4 Mat4::asymmetricPerspective(const AsymmetricPerspective& options) {
    const auto inView = [](float angle) { return angle > -Math::kHalfPi && angle < Math::kHalfPi; };
    if (!inView(options.left) || !inView(options.right) || !inView(options.bottom) || !inView(options.top) || !(options.left < options.right) || !(options.bottom < options.top)) {
        throw std::invalid_argument("The angles of an asymmetric perspective must lie within a quarter turn of the direction it looks along, with left below right and bottom below top.");
    }

    checkDepthPlanes(options.nearPlane, options.farPlane, "an asymmetric perspective");
    return projectTangents(std::tan(options.left), std::tan(options.right), std::tan(options.bottom), std::tan(options.top), options.nearPlane, options.farPlane, options.depthRange, options.reversedDepth);
}

// Depth runs straight from the near plane to the far plane, which the camera sees at the distances `nearPlane` and `farPlane` along negative z.
Mat4 Mat4::orthographic(const Orthographic& options) {
    const bool finite = std::isfinite(options.left) && std::isfinite(options.right) && std::isfinite(options.bottom) && std::isfinite(options.top) && std::isfinite(options.nearPlane) && std::isfinite(options.farPlane);
    if (!finite || options.left == options.right || options.bottom == options.top || options.nearPlane == options.farPlane) {
        throw std::invalid_argument("The bounds of an orthographic projection must be finite numbers, with left apart from right, bottom apart from top and the near plane apart from the far plane.");
    }

    const auto [nearDepth, farDepth] = getDepthEnds(options.depthRange, options.reversedDepth);
    const float scale = (nearDepth - farDepth) / (options.farPlane - options.nearPlane);
    const float offset = nearDepth + scale * options.nearPlane;
    const float width = options.right - options.left;
    const float height = options.top - options.bottom;

    Mat4 projection;
    projection.values = {2.0F / width, 0.0F, 0.0F, 0.0F, 0.0F, 2.0F / height, 0.0F, 0.0F, 0.0F, 0.0F, scale, 0.0F, -(options.right + options.left) / width, -(options.top + options.bottom) / height, offset, 1.0F};
    return projection;
}

std::array<float, 12> Mat4::getMinors() const noexcept {
    const std::array<float, 16>& a = values;
    return {a[0] * a[5] - a[1] * a[4], a[0] * a[6] - a[2] * a[4], a[0] * a[7] - a[3] * a[4], a[1] * a[6] - a[2] * a[5], a[1] * a[7] - a[3] * a[5], a[2] * a[7] - a[3] * a[6], a[8] * a[13] - a[9] * a[12], a[8] * a[14] - a[10] * a[12], a[8] * a[15] - a[11] * a[12], a[9] * a[14] - a[10] * a[13], a[9] * a[15] - a[11] * a[13], a[10] * a[15] - a[11] * a[14]};
}

float Mat4::getDeterminant() const noexcept {
    const std::array<float, 12> b = getMinors();
    return b[0] * b[11] - b[1] * b[10] + b[2] * b[9] + b[3] * b[8] - b[4] * b[7] + b[5] * b[6];
}

bool Mat4::isInvertible() const noexcept {
    if (isAffine()) {
        return getLinear().isInvertible();
    }

    const float determinant = getDeterminant();
    return std::isfinite(determinant) && determinant != 0.0F;
}

// Each element of the inverse is a cofactor built from the products of pairs of elements, divided by the determinant.
Mat4 Mat4::getInverse() const {
    if (!isInvertible()) {
        throw std::invalid_argument("The matrix has no inverse, because it squashes space flat and its determinant is zero.");
    }

    const std::array<float, 16>& a = values;
    const std::array<float, 12> b = getMinors();
    const float inverseDeterminant = 1.0F / getDeterminant();

    Mat4 inverse;
    inverse.values = {
        (a[5] * b[11] - a[6] * b[10] + a[7] * b[9]) * inverseDeterminant, (a[2] * b[10] - a[1] * b[11] - a[3] * b[9]) * inverseDeterminant, (a[13] * b[5] - a[14] * b[4] + a[15] * b[3]) * inverseDeterminant, (a[10] * b[4] - a[9] * b[5] - a[11] * b[3]) * inverseDeterminant, (a[6] * b[8] - a[4] * b[11] - a[7] * b[7]) * inverseDeterminant, (a[0] * b[11] - a[2] * b[8] + a[3] * b[7]) * inverseDeterminant, (a[14] * b[2] - a[12] * b[5] - a[15] * b[1]) * inverseDeterminant, (a[8] * b[5] - a[10] * b[2] + a[11] * b[1]) * inverseDeterminant, (a[4] * b[10] - a[5] * b[8] + a[7] * b[6]) * inverseDeterminant, (a[1] * b[8] - a[0] * b[10] - a[3] * b[6]) * inverseDeterminant, (a[12] * b[4] - a[13] * b[2] + a[15] * b[0]) * inverseDeterminant, (a[9] * b[2] - a[8] * b[4] - a[11] * b[0]) * inverseDeterminant, (a[5] * b[7] - a[4] * b[9] - a[6] * b[6]) * inverseDeterminant, (a[0] * b[9] - a[1] * b[7] + a[2] * b[6]) * inverseDeterminant, (a[13] * b[1] - a[12] * b[3] - a[14] * b[0]) * inverseDeterminant, (a[8] * b[3] - a[9] * b[1] + a[10] * b[0]) * inverseDeterminant,
    };
    return inverse;
}

Mat4 Mat4::getAffineInverse() const {
    if (!isAffine()) {
        throw std::invalid_argument("The matrix is not affine, because its last row is not 0, 0, 0 and 1.");
    }

    const Mat3 inverseLinear = getLinear().getInverse();
    return fromLinear(inverseLinear, -(inverseLinear * getTranslation()));
}

Mat3 Mat4::getNormalMatrix() const {
    return getLinear().getInverse().getTransposed();
}

Mat4::Decomposition Mat4::decompose() const {
    if (!isAffine()) {
        throw std::invalid_argument("Only an affine matrix, whose last row is 0, 0, 0 and 1, splits into translation, rotation and scale.");
    }

    const Mat3 linear = getLinear();
    Vec3 scale{linear.getColumn(0).getLength(), linear.getColumn(1).getLength(), linear.getColumn(2).getLength()};
    if (scale.x == 0.0F || scale.y == 0.0F || scale.z == 0.0F || !scale.isFinite()) {
        throw std::invalid_argument("The matrix scales an axis to zero or to infinity, so it holds no rotation.");
    }

    if (linear.getDeterminant() < 0.0F) {
        scale.x = -scale.x;
    }

    const Mat3 rotation = Mat3::fromColumns(linear.getColumn(0) / scale.x, linear.getColumn(1) / scale.y, linear.getColumn(2) / scale.z);
    return {getTranslation(), Quat::fromMatrix(rotation), scale};
}

// The new near plane keeps the points in front of `plane`, and the far plane tilts so it still passes through the far corner of the view that the plane faces, which keeps as much of the range of depth as the plane allows.
Mat4 Mat4::getObliqueNearPlane(const Plane& plane, DepthRange depthRange, bool reversedDepth) const {
    const Vec4 coefficients = plane.getCoefficients();
    if (!(coefficients.w < 0.0F)) {
        throw std::invalid_argument("An oblique near plane must face away from the camera, so the camera must lie behind the plane.");
    }

    const Mat4 inverse = getInverse();
    const Vec4 clipPlane = inverse.getTransposed() * coefficients;
    const auto [nearDepth, farDepth] = getDepthEnds(depthRange, reversedDepth);
    const Vec4 corner = inverse * Vec4{Math::sign(clipPlane.x), Math::sign(clipPlane.y), farDepth, 1.0F};
    const float reach = Vec4::dot(coefficients, corner);
    if (!(reach > 0.0F) || !std::isfinite(reach)) {
        throw std::invalid_argument("An oblique near plane must cross the view before its far end.");
    }

    const Vec4 depthRow = getRow(3) * nearDepth + coefficients * ((farDepth - nearDepth) / reach);
    Mat4 oblique = *this;
    oblique.values[2] = depthRow.x;
    oblique.values[6] = depthRow.y;
    oblique.values[10] = depthRow.z;
    oblique.values[14] = depthRow.w;
    return oblique;
}

bool Mat4::approximately(const Mat4& lhs, const Mat4& rhs, float epsilon) noexcept {
    for (std::size_t index = 0; index < lhs.values.size(); ++index) {
        if (!Math::approximately(lhs.values[index], rhs.values[index], epsilon)) {
            return false;
        }
    }

    return true;
}

} // namespace haylen::math
