#pragma once

#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::ui {

// How a node draws on top of the place its layout gives it. The offset moves the node and its children together with their input areas, while the scale around the center of the node, the opacity and the tint change only how they look. Children inherit the transform of their parents, and tweens animate it natively through the transform handle of a GUI node.
struct Transform {
    math::Vec2 offset{};
    math::Vec2 scale{1.0F, 1.0F};
    float opacity = 1.0F;
    math::Color tint = math::Color::white();

    // Returns whether scale, opacity or tint change the drawing, which is when the vertices of the node need reshaping.
    [[nodiscard]] bool isReshaping() const noexcept {
        return scale != math::Vec2{1.0F, 1.0F} || opacity != 1.0F || tint != math::Color::white();
    }
};

} // namespace haylen::ui
