#pragma once

#include <cstddef>
#include <memory>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"
#include "sokol_gfx.h"

namespace haylen::graphics2d {

// The instance buffer behind a static sprite batch, buried in the device graveyard when the last handle goes away.
struct StaticBatchResource {
    sg_buffer buffer{};
    std::size_t count = 0;
    graphics::Texture texture;
    math::Rect bounds{};
    std::weak_ptr<graphics::ResourceGraveyard> graveyard;

    StaticBatchResource() = default;
    StaticBatchResource(const StaticBatchResource&) = delete;
    StaticBatchResource& operator=(const StaticBatchResource&) = delete;
    ~StaticBatchResource();
};

} // namespace haylen::graphics2d
