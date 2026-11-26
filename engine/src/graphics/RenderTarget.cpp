#include "haylen/graphics/RenderTarget.hpp"

#include <format>
#include <stdexcept>

#include "graphics/RenderTargetResource.hpp"

namespace haylen::graphics {

const Texture& RenderTarget::getTexture() const noexcept {
    static const Texture& kEmpty = *new const Texture();
    return resource ? resource->textures.front() : kEmpty;
}

const Texture& RenderTarget::getTexture(std::size_t index) const {
    if (!resource || index >= resource->textures.size()) {
        throw std::out_of_range(std::format("The render target has no color {}.", index + 1));
    }

    return resource->textures[index];
}

std::size_t RenderTarget::getColorCount() const noexcept {
    return resource ? resource->textures.size() : 0;
}

const Texture& RenderTarget::getDepthTexture() const noexcept {
    static const Texture& kEmpty = *new const Texture();
    return resource ? resource->depthTexture : kEmpty;
}

const RenderTarget::Options& RenderTarget::getOptions() const noexcept {
    static const Options& kEmpty = *new const Options();
    return resource ? resource->options : kEmpty;
}

} // namespace haylen::graphics
