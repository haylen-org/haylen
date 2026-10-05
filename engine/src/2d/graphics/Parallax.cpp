#include "haylen/2d/graphics/Parallax.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Sprite.hpp"

namespace haylen::graphics2d {

float Parallax::firstCopy(float start, float origin, float step) noexcept {
    return origin + std::floor((start - origin) / step) * step;
}

void Parallax::update(float deltaSeconds) noexcept {
    scrolled += autoscroll * deltaSeconds;
}

math::Vec2 Parallax::getOffset(const Camera& camera) const noexcept {
    const math::Vec2 view = limits ? limits->clamp(camera.getRenderPosition()) : camera.getRenderPosition();
    const math::Vec2 follow{1.0F - scrollScale.x, 1.0F - scrollScale.y};
    return scrolled + view * follow;
}

void Parallax::draw(Renderer& renderer, const Camera& camera, const DrawOrder& order) const {
    if (!texture.isValid()) {
        throw std::logic_error("A parallax layer needs a texture to draw.");
    }

    const math::Rect part = source.isEmpty() ? math::Rect{0.0F, 0.0F, texture.getSize().x, texture.getSize().y} : source;
    const math::Vec2 drawn = size.isZero() ? part.getSize() : size;
    const math::Vec2 step{repeatSize.x > 0.0F ? repeatSize.x : drawn.x, repeatSize.y > 0.0F ? repeatSize.y : drawn.y};
    if (step.x <= 0.0F || step.y <= 0.0F) {
        throw std::invalid_argument("A parallax layer needs a positive size to repeat.");
    }

    // Repeated axes cover the part of the world the canvas shows with copies on a grid anchored at the layer origin, counted rather than stepped so far positions never stall on float precision.
    const math::Vec2 origin = position + getOffset(camera);
    const math::Rect visible = renderer.getCanvasBounds();
    const float left = repeatX ? firstCopy(visible.getLeft(), origin.x, step.x) : origin.x;
    const float top = repeatY ? firstCopy(visible.getTop(), origin.y, step.y) : origin.y;
    const int columns = repeatX ? static_cast<int>(std::ceil((visible.getRight() - left) / step.x)) : 1;
    const int rows = repeatY ? static_cast<int>(std::ceil((visible.getBottom() - top) / step.y)) : 1;

    // Every copy is a sprite of its own, which the renderer merges into one draw call with its neighbours, so a frame allocates nothing for them.
    Sprite copy{.texture = texture, .source = part, .size = drawn, .pivot = {}, .color = color, .order = order};
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            copy.position = {left + static_cast<float>(column) * step.x, top + static_cast<float>(row) * step.y};
            renderer.draw(copy);
        }
    }
}

} // namespace haylen::graphics2d
