#pragma once

#include <cstdint>

namespace haylen::graphics {

// Where a render pass draws, which decides the pixel formats of the pipelines it uses: the swapchain, one offscreen color target, the four targets a lit canvas fills at once, or the light map.
enum class PassTarget : std::uint8_t {
    Swapchain,
    Offscreen,
    LitScene,
    LightMap,
};

} // namespace haylen::graphics
