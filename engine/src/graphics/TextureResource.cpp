#include "graphics/TextureResource.hpp"

namespace haylen::graphics {

debug::ObjectCounter& TextureResource::textures = *new debug::ObjectCounter("Texture", debug::ObjectCounter::Kind::Native);
debug::ObjectCounter& TextureResource::targets = *new debug::ObjectCounter("RenderTarget", debug::ObjectCounter::Kind::Native);

TextureResource::~TextureResource() {
    if (const auto owner = graveyard.lock()) {
        owner->bury(image, view);
    }
}

} // namespace haylen::graphics
