#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::graphics2d {

// The instance buffer behind a static sprite batch, buried in the device graveyard when the last handle goes away. Batches count themselves with the bytes of their buffer as `StaticSpriteBatch` in the debug statistics.
struct StaticBatchResource {
    static debug::ObjectCounter& counter;

    gpu::Device::Buffer buffer;
    std::size_t count = 0;
    graphics::Texture texture;
    math::Rect bounds{};
    std::weak_ptr<graphics::ResourceGraveyard> graveyard;

    // The submission of the renderer that last kept the batch alive until it ends.
    std::uint64_t retainEpoch = 0;
    debug::TrackedObject tracked{counter};

    StaticBatchResource() = default;
    StaticBatchResource(const StaticBatchResource&) = delete;
    StaticBatchResource& operator=(const StaticBatchResource&) = delete;
    ~StaticBatchResource();
};

} // namespace haylen::graphics2d
