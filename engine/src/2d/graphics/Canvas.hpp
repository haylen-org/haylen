#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// One canvas of a frame, with the ranges of the draw items, commands, lights, occluder edges and metaballs that belong to it.
struct Canvas {
    enum class Kind : std::uint8_t {
        World,
        Screen,
        Target,
    };

    Kind kind = Kind::Screen;
    Renderer::CanvasOptions options{};
    graphics::RenderTarget target;
    math::Vec2 viewSize{};
    math::Transform2D view{};

    // The part of its destination the canvas covers, from (0, 0) at the top-left to (1, 1) at the bottom-right.
    math::Rect frame{0.0F, 0.0F, 1.0F, 1.0F};

    // The capture the canvas renders into, counted from 1, or 0 for the screen.
    std::size_t capture = 0;
    std::size_t itemBegin = 0;
    std::size_t itemEnd = 0;
    std::size_t sceneBegin = 0;
    std::size_t sceneEnd = 0;
    std::size_t lightBegin = 0;
    std::size_t lightEnd = 0;
    std::size_t segmentBegin = 0;
    std::size_t segmentEnd = 0;
    std::size_t metaballBegin = 0;
    std::size_t metaballEnd = 0;
    std::size_t litIndex = 0;

    // The first of the shades of the post-processing materials, and the post target that holds the image the last material draws.
    std::size_t postShade = 0;
    std::size_t postImage = 0;

    // Returns the part of the destination that the area covers, both in the same coordinates.
    [[nodiscard]] static math::Rect frameOf(const math::Rect& area, const math::Rect& destination) noexcept {
        const math::Vec2 size = destination.getSize();
        return {(area.x - destination.x) / size.x, (area.y - destination.y) / size.y, area.width / size.x, area.height / size.y};
    }

    // Returns the area of the world or of the design space the canvas shows.
    [[nodiscard]] math::Rect getWorldBounds() const noexcept {
        const math::Transform2D inverse = view.getInverse();
        const std::array<math::Vec2, 4> corners{inverse.apply(math::Vec2{}), inverse.apply(math::Vec2{viewSize.x, 0.0F}), inverse.apply(viewSize), inverse.apply(math::Vec2{0.0F, viewSize.y})};
        return math::Geometry::bounds(corners);
    }

    // Lit or post-processed canvases render offscreen first and reach their destination through the composite pass.
    [[nodiscard]] bool isComposited() const noexcept {
        return kind != Kind::Screen && (options.ambientLight || options.postProcess);
    }
    [[nodiscard]] bool isLit() const noexcept {
        return kind != Kind::Screen && options.ambientLight.has_value();
    }
    [[nodiscard]] bool hasPostMaterials() const noexcept {
        return options.postProcess && !options.postProcess->materials.empty();
    }
};

} // namespace haylen::graphics2d
