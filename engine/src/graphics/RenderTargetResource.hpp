#pragma once

#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics {

// The texture behind a render target handle, which passes draw into and draws sample.
struct RenderTargetResource {
    Texture texture;
};

} // namespace haylen::graphics
