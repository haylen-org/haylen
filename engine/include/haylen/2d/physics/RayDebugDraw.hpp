#pragma once

#include <optional>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::physics2d {

// Collects cast rays with their hits and draws them over the scene in world units: each ray up to its hit or its end, a dot on the hit and a short line along its normal. Drawing clears the rays, so a frame shows the rays of the frame before it.
class RayDebugDraw final {
  public:
    struct Hit {
        math::Vec2 point{};
        math::Vec2 normal{};
    };

    void add(math::Vec2 from, math::Vec2 to, const std::optional<Hit>& hit);
    void clear() noexcept {
        rays.clear();
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return rays.size();
    }

    // Draws the collected rays and clears them.
    void draw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& order = {});

    // Draws one ray with its optional hit right away.
    static void drawRay(graphics2d::Renderer& renderer, math::Vec2 from, math::Vec2 to, const std::optional<Hit>& hit, const graphics2d::DrawOrder& order = {});

  private:
    struct Ray {
        math::Vec2 from{};
        math::Vec2 to{};
        std::optional<Hit> hit;
    };

    static constexpr float kLineWidth = 1.5F;
    static constexpr float kHitRadius = 3.0F;
    static constexpr float kNormalLength = 16.0F;
    static const math::Color kMissColor;
    static const math::Color kHitColor;
    static const math::Color kNormalColor;

    std::vector<Ray> rays;
};

} // namespace haylen::physics2d
