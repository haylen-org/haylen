#pragma once

#include <memory>

#include "graphics/VectorImageResource.hpp"

struct NSVGimage;

namespace haylen::graphics {

// An SVG document read into the curves of nanosvg, whose size is its view box, its width and height, or the extent of what it draws.
class SvgDocument final : public VectorImageResource {
  public:
    SvgDocument(std::unique_ptr<NSVGimage, void (*)(NSVGimage*)> parsed, math::Vec2 documentSize) noexcept;

    [[nodiscard]] Image rasterize(float scale) const override;

  private:
    std::unique_ptr<NSVGimage, void (*)(NSVGimage*)> document;
};

} // namespace haylen::graphics
