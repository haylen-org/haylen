#include "graphics/RenderTargetResource.hpp"

namespace haylen::graphics {

RenderTargetResource::~RenderTargetResource() {
    if (const auto owner = graveyard.lock()) {
        owner->buryView(attachment);
    }
}

} // namespace haylen::graphics
