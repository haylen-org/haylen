#pragma once

#include <cstdint>

namespace haylen::graphics {

// Where a render pass draws, which decides the pixel formats and the write masks of the pipelines it uses: the swapchain of an opaque window, which keeps its alpha at 1, the swapchain of a transparent window, one offscreen color target, the four targets a lit canvas fills at once, or the light map.
enum class PassTarget : std::uint8_t {
    Swapchain,
    TransparentSwapchain,
    Offscreen,
    LitScene,
    LightMap,
};

} // namespace haylen::graphics
