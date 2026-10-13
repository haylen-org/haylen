#pragma once

#include <cstddef>
#include <cstdint>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics2d {

// An open group or mask: the canvas it draws over, where its draws start in the lists of the frame, the parts of the state of that canvas it set aside, and how it composites when it ends.
struct GroupScope {
    enum class Stage : std::uint8_t {
        Group,
        MaskShape,
        MaskContent,
    };

    Stage stage = Stage::Group;
    std::size_t parent = 0;
    std::size_t itemStart = 0;
    std::size_t metaballStart = 0;
    std::size_t target = 0;
    std::size_t clipStart = 0;
    std::size_t layerOffsetStart = 0;
    int layerOffset = 0;
    float opacity = 1.0F;
    bool outside = false;
    DrawOrder order{};

    // The image the shape of a mask drew, which its content shows through.
    graphics::Texture shape;
};

} // namespace haylen::graphics2d
