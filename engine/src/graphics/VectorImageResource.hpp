#pragma once

#include <atomic>
#include <cstdint>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {

// The picture behind a vector image handle, an SVG document or a color glyph of a font, which stays read-only while rasterizers on any thread read it. The debug statistics count vector images apart.
class VectorImageResource {
  public:
    virtual ~VectorImageResource();

    VectorImageResource(const VectorImageResource&) = delete;
    VectorImageResource& operator=(const VectorImageResource&) = delete;

    [[nodiscard]] math::Vec2 getSize() const noexcept {
        return size;
    }
    [[nodiscard]] std::uint32_t getId() const noexcept {
        return id;
    }

    // Rasterizes the whole picture at a scale into straight RGBA pixels of the size `VectorImage::getRasterSize` gives, with the colors of the edges spread into the transparent pixels around them. Safe on any thread.
    [[nodiscard]] virtual Image rasterize(float scale) const = 0;

  protected:
    explicit VectorImageResource(math::Vec2 imageSize) noexcept : size(imageSize) {}

  private:
    static debug::ObjectCounter& counter;
    static std::atomic<std::uint32_t> nextId;

    math::Vec2 size;
    std::uint32_t id = nextId.fetch_add(1);
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::graphics
