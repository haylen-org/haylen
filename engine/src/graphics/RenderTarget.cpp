#include "haylen/graphics/RenderTarget.hpp"

#include "graphics/RenderTargetResource.hpp"

namespace haylen::graphics {

const Texture& RenderTarget::getTexture() const noexcept {
    static const Texture kEmpty;
    return resource ? resource->texture : kEmpty;
}

} // namespace haylen::graphics
