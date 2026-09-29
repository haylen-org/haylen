#include "haylen/graphics/RenderTarget.hpp"

#include "graphics/RenderTargetResource.hpp"

namespace haylen::graphics {

const Texture& RenderTarget::getTexture() const noexcept {
    static const Texture& kEmpty = *new const Texture();
    return resource ? resource->texture : kEmpty;
}

} // namespace haylen::graphics
