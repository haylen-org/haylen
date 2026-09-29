#include "haylen/2d/graphics/StaticSpriteBatch.hpp"

#include "2d/graphics/StaticBatchResource.hpp"

namespace haylen::graphics2d {

std::size_t StaticSpriteBatch::size() const noexcept {
    return resource ? resource->count : 0;
}

math::Rect StaticSpriteBatch::getBounds() const noexcept {
    return resource ? resource->bounds : math::Rect{};
}

const graphics::Texture& StaticSpriteBatch::getTexture() const noexcept {
    static const graphics::Texture& kEmpty = *new const graphics::Texture();
    return resource ? resource->texture : kEmpty;
}

} // namespace haylen::graphics2d
