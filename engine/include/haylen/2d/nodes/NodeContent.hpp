#pragma once

#include <memory>
#include <string>
#include <variant>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::text {
class Font;
}

namespace haylen::particles2d {
class Emitter;
class System;
} // namespace haylen::particles2d

namespace haylen::nodes2d {

// What a node draws at its place. The node places the content: its world position, rotation and scale replace the position, rotation and scale of a sprite, of text and of a frame, its world color multiplies their colors, and its layer adds to the layer of their order. Particles spawn at the place of the node.
struct NodeContent {
    // A block of text in a font, the default font of the engine when the font is empty.
    struct Text {
        std::shared_ptr<text::Font> font;
        std::string content;
        text::Style style;
        graphics2d::DrawOrder order;
    };

    // A nine-slice frame of a size around its pivot.
    struct Frame {
        graphics2d::NineSlice slice;
        math::Vec2 size{};
        math::Vec2 pivot{0.5F, 0.5F};
        math::Color color = math::Color::white();
        float borderScale = 1.0F;
        graphics2d::DrawOrder order;
    };

    using Value = std::variant<std::monostate, graphics2d::Sprite, Text, Frame, std::shared_ptr<particles2d::Emitter>, std::shared_ptr<particles2d::System>>;

    Value value;

    // The package path of the font of text or of the effect file of particles, which saving writes, empty for the default font and for particles made in code.
    std::string path;

    [[nodiscard]] bool isEmpty() const noexcept {
        return std::holds_alternative<std::monostate>(value);
    }
};

} // namespace haylen::nodes2d
