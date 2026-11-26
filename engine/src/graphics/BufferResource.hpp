#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Buffer.hpp"

namespace haylen::graphics {

// The GPU buffer behind a buffer handle, buried in the device graveyard when the last handle goes away. The debug statistics count buffers with their bytes.
struct BufferResource {
    static debug::ObjectCounter& counter;

    gpu::Device::Buffer buffer;
    std::size_t size = 0;
    Buffer::Usage usage{};
    Buffer::Update update = Buffer::Update::Immutable;
    std::string label;
    std::weak_ptr<ResourceGraveyard> graveyard;
    debug::TrackedObject tracked{counter};

    BufferResource() = default;
    BufferResource(const BufferResource&) = delete;
    BufferResource& operator=(const BufferResource&) = delete;
    ~BufferResource();
};

} // namespace haylen::graphics
