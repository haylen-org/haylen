#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Traces the outlines of the areas of a sampled field that reach a threshold. Outlines are closed, outer outlines wind with a positive Geometry::signedArea and holes with a negative one, like the results of Polygon.
class MarchingSquares final {
  public:
    // Sample (x, y) lies at origin + (x, y) * spacing.
    struct Options {
        float threshold = 0.5F;
        float spacing = 1.0F;
        Vec2 origin{};
    };

    // Traces a field of width times height samples stored row by row. Areas that reach the edge of the grid close along the outermost samples. Throws std::invalid_argument when the value count does not match the size.
    [[nodiscard]] static std::vector<std::vector<Vec2>> trace(std::span<const float> values, int width, int height, const Options& options = kDefaultOptions);

    // Traces the pixels of a bitmap that are not zero. Pixel (x, y) covers the square from origin + (x, y) * spacing to origin + (x + 1, y + 1) * spacing, and outlines cut diagonally across the corners of the pixels.
    [[nodiscard]] static std::vector<std::vector<Vec2>> traceBitmap(std::span<const std::uint8_t> pixels, int width, int height, float spacing = 1.0F, Vec2 origin = {});

  private:
    class Tracer;

    static const Options kDefaultOptions;
};

} // namespace haylen::math
