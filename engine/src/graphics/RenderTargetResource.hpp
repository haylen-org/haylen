#pragma once

#include <memory>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/graphics/Texture.hpp"
#include "sokol_gfx.h"

namespace haylen::graphics {

// The color attachment view behind a render target handle, next to the texture it renders into.
struct RenderTargetResource {
    Texture texture;
    sg_view attachment{};
    std::weak_ptr<ResourceGraveyard> graveyard;

    RenderTargetResource() = default;
    RenderTargetResource(const RenderTargetResource&) = delete;
    RenderTargetResource& operator=(const RenderTargetResource&) = delete;
    ~RenderTargetResource();
};

} // namespace haylen::graphics
