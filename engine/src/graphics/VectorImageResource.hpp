#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/math/Vec2.hpp"

struct NSVGimage;

namespace haylen::graphics {

// The document behind a vector image handle, whose curves stay read-only while rasterizers on any thread read them. The debug statistics count vector images apart.
struct VectorImageResource {
    static debug::ObjectCounter& counter;
    static std::atomic<std::uint32_t> nextId;

    std::unique_ptr<NSVGimage, void (*)(NSVGimage*)> document{nullptr, nullptr};
    math::Vec2 size{};
    std::uint32_t id = nextId.fetch_add(1);
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::graphics
