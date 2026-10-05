#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <utility>

#include "haylen/graphics/Image.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {

struct VectorImageResource;

// Shared handle to a vector image read from an SVG document: paths and basic shapes with their fills and strokes, line joins, caps and dashes, linear and radial gradients, transforms, groups, opacity and the view box. The color `currentColor` paints white, so the color of a draw tints the parts that use it. It rasterizes at any scale on any thread.
class VectorImage final {
  public:
    VectorImage() = default;
    explicit VectorImage(std::shared_ptr<VectorImageResource> value) noexcept : resource(std::move(value)) {}

    // Reads an SVG document, whose size is its view box, its width and height, or the extent of what it draws. Throws `std::invalid_argument` when the bytes are not an SVG document or it has no size.
    [[nodiscard]] static VectorImage parse(std::span<const std::uint8_t> bytes);

    [[nodiscard]] bool isValid() const noexcept {
        return resource != nullptr;
    }

    // The size the document gives itself in its units, which a draw without a size of its own takes.
    [[nodiscard]] math::Vec2 getSize() const;

    // Identifies the image for as long as it lives, which keys its rasters.
    [[nodiscard]] std::uint32_t getId() const;

    // Returns the size in pixels of the raster at a scale, which covers the whole image.
    [[nodiscard]] static std::pair<int, int> getRasterSize(math::Vec2 size, float scale) noexcept;

    // Rasterizes the whole image at a scale into straight RGBA pixels of the size `getRasterSize` gives, with the colors of the edges spread into the transparent pixels around them so filtering never darkens them. Safe on any thread.
    [[nodiscard]] Image rasterize(float scale) const;

    [[nodiscard]] const std::shared_ptr<VectorImageResource>& getResource() const noexcept {
        return resource;
    }
    [[nodiscard]] bool operator==(const VectorImage& other) const noexcept {
        return resource == other.resource;
    }

  private:
    std::shared_ptr<VectorImageResource> resource;
};

} // namespace haylen::graphics
