#include "platform/SurfaceCheck.hpp"

#include <cmath>

#include "haylen/core/Log.hpp"

namespace haylen::platform {

math::Vec2 SurfaceCheck::toPixels(math::Vec2 points, float scale) noexcept {
    return {std::trunc(points.x * scale), std::trunc(points.y * scale)};
}

bool SurfaceCheck::matches(const Sizes& sizes) noexcept {
    return sizes.view == sizes.window && toPixels(sizes.view, sizes.scale) == sizes.drawable;
}

bool SurfaceCheck::check(const Sizes& sizes) {
    const bool wasParted = parted;
    parted = !matches(sizes);
    if (!parted || wasParted) {
        return false;
    }
    core::Log::error("The view of the app covers {} by {} points of its window of {} by {}, and its drawable has {} by {} pixels for {} by {} at a scale of {}.", sizes.view.x, sizes.view.y, sizes.window.x, sizes.window.y, sizes.drawable.x, sizes.drawable.y, toPixels(sizes.view, sizes.scale).x, toPixels(sizes.view, sizes.scale).y, sizes.scale);
    return true;
}

} // namespace haylen::platform
