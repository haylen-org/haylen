#include "graphics/RenderTargetResource.hpp"

#include "graphics/TextureResource.hpp"

namespace haylen::graphics {

RenderTargetResource::~RenderTargetResource() {
    const auto owner = graveyard.lock();
    if (!owner) {
        return;
    }

    for (const gpu::Device::Image image : samples) {
        owner->bury(image);
    }

    if (depth.isValid()) {
        owner->bury(depth);
    }
}

gpu::Device::Image RenderTargetResource::getColorAttachment(std::size_t index) const {
    return samples.empty() ? textures[index].getResource()->image : samples[index];
}

gpu::Device::Image RenderTargetResource::getDepthAttachment() const {
    if (depth.isValid()) {
        return depth;
    }

    return depthTexture.isValid() ? depthTexture.getResource()->image : gpu::Device::Image{};
}

} // namespace haylen::graphics
