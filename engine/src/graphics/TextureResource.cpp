#include "graphics/TextureResource.hpp"

namespace haylen::graphics {

debug::ObjectCounter TextureResource::textures("Texture", debug::ObjectCounter::Kind::Native);
debug::ObjectCounter TextureResource::targets("RenderTarget", debug::ObjectCounter::Kind::Native);

TextureResource::~TextureResource() {
    if (const auto owner = graveyard.lock()) {
        owner->bury(image, view);
    }
}

} // namespace haylen::graphics
