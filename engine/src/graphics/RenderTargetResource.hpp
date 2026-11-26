#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics {

// The images behind a render target handle: the texture of each color and of a kept depth, which draws sample, and the attachments that no texture holds, the samples of each color and a depth without a texture, which the resource buries in the device graveyard when the last handle goes away.
struct RenderTargetResource {
    std::vector<Texture> textures;
    Texture depthTexture;
    std::vector<gpu::Device::Image> samples;
    gpu::Device::Image depth;
    RenderTarget::Options options;
    std::weak_ptr<ResourceGraveyard> graveyard;

    RenderTargetResource() = default;
    RenderTargetResource(const RenderTargetResource&) = delete;
    RenderTargetResource& operator=(const RenderTargetResource&) = delete;
    ~RenderTargetResource();

    // The image that the color at the index draws into: its samples when the target has several, and its texture otherwise.
    [[nodiscard]] gpu::Device::Image getColorAttachment(std::size_t index) const;

    // The image of the depth attachment: the depth without a texture or the samples of a kept depth when the target has them, the texture of the depth otherwise, and none for a target without depth.
    [[nodiscard]] gpu::Device::Image getDepthAttachment() const;

    // Whether a pass into the target loses its samples once it ends, so no later pass keeps what an earlier one drew there.
    [[nodiscard]] bool losesSamples() const noexcept {
        return options.memoryless && options.samples > 1;
    }
};

} // namespace haylen::graphics
