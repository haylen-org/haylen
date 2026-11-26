#include "haylen/math/Ray3D.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/math/Vec4.hpp"

namespace haylen::math {

Ray3D Ray3D::fromView(const Mat4& viewProjection, Vec2 point, Mat4::DepthRange depthRange, bool reversedDepth) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        throw std::invalid_argument("The point of a view must have coordinates that are finite numbers.");
    }

    const Mat4 inverse = viewProjection.getInverse();
    const auto [nearDepth, farDepth] = Mat4::getDepthEnds(depthRange, reversedDepth);
    const Vec4 onNear = inverse * Vec4{point.x, point.y, nearDepth, 1.0F};
    if (!std::isfinite(onNear.w) || onNear.w == 0.0F) {
        throw std::invalid_argument("The matrix is not the projection and view of a camera, because it puts the point of the view at no place on the near plane.");
    }

    // The ray runs through clip space from the near end of depth toward the far end, and its direction is how that line leaves the near plane in the world, so a far plane at infinity needs no point there.
    const Vec4 towardFar = inverse * Vec4{0.0F, 0.0F, farDepth - nearDepth, 0.0F};
    const Vec3 direction = towardFar.getXyz() * onNear.w - onNear.getXyz() * towardFar.w;
    return {onNear.getXyz() / onNear.w, direction.getNormalized()};
}

} // namespace haylen::math
