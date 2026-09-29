#pragma once

#include <optional>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

class Camera;
class Renderer;

// A layer that scrolls at its own rate as the camera moves, which suggests depth. It draws a texture that can repeat on each axis to fill the view, scrolls by itself over time and stops following the camera while the camera is outside its limits. The offset also moves anything else, such as a static batch, with the layer.
class Parallax final {
  public:
    graphics::Texture texture;

    // The part of the texture to draw, or the whole texture when empty.
    math::Rect source{};

    // Where the layer sits in the world while the camera shows the world origin.
    math::Vec2 position{};

    // The drawn size of one copy, or the size of the source when zero.
    math::Vec2 size{};

    // How much the layer follows the world on each axis: 1 moves with the world, 0 stays still on the screen and values between look farther away.
    math::Vec2 scrollScale{1.0F, 1.0F};
    bool repeatX = false;
    bool repeatY = false;

    // The distance between copies, or the drawn size when zero.
    math::Vec2 repeatSize{};

    // World units per second the layer scrolls by itself.
    math::Vec2 autoscroll{};

    // The camera positions, in world units, where the layer scrolls. Outside them the layer stays as it was at the edge.
    std::optional<math::Rect> limits;
    math::Color color = math::Color::white();

    void update(float deltaSeconds) noexcept;

    // Returns how far the layer is moved from where the world would put it, in world units.
    [[nodiscard]] math::Vec2 getOffset(const Camera& camera) const noexcept;
    [[nodiscard]] math::Vec2 getScrolled() const noexcept {
        return scrolled;
    }

    // Draws the texture at the offset position, repeated over the part of the world the camera shows on the axes that repeat.
    void draw(Renderer& renderer, const Camera& camera, const math::Rect& screen, const DrawOrder& order = {}) const;

  private:
    // Returns the first copy at or before the start of the visible range, on a grid of the step anchored at the origin.
    [[nodiscard]] static float firstCopy(float start, float origin, float step) noexcept;

    math::Vec2 scrolled{};
};

} // namespace haylen::graphics2d
