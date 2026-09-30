#pragma once

#include <cstdint>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::spatial2d {

// Finds what lies under a point of the screen, such as the cursor or a touch, through the camera of a world canvas. Points are in design coordinates like pointer positions, and the screen is the visible area of the viewport, where world canvases draw.
class ScreenPicker final {
  public:
    ScreenPicker(const graphics2d::Camera& viewCamera, const math::Rect& viewScreen) noexcept : camera(viewCamera), screen(viewScreen) {}

    [[nodiscard]] math::Vec2 toWorld(math::Vec2 point) const noexcept {
        return camera.screenToWorld(point, screen);
    }

    // Returns the ray from the world point at the center of the view to the world point under the screen point, for aiming from the view.
    [[nodiscard]] math::Ray toRay(math::Vec2 point) const noexcept {
        return math::Ray::between(toWorld(camera.getViewRect(screen).getCenter()), toWorld(point));
    }

    // Fills `ids` with the entries of a spatial structure whose bounds contain the world point under the screen point.
    template <typename Structure> void pick(const Structure& structure, math::Vec2 point, std::vector<std::uint64_t>& ids) const {
        structure.queryPoint(toWorld(point), ids);
    }

  private:
    const graphics2d::Camera& camera;
    math::Rect screen;
};

} // namespace haylen::spatial2d
