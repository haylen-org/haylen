#include "haylen/2d/particles/ImageShape.hpp"

#include <stdexcept>

#include "haylen/graphics/Image.hpp"

namespace haylen::particles2d {

ImageShape::ImageShape(const graphics::Image& image, const Options& options) {
    if (!(options.alphaThreshold > 0.0F && options.alphaThreshold <= 1.0F)) {
        throw std::invalid_argument("The alpha threshold of an image shape must be above 0 and at most 1.");
    }
    const math::Rect whole{0.0F, 0.0F, static_cast<float>(image.getWidth()), static_cast<float>(image.getHeight())};
    const math::Rect source = options.source.isEmpty() ? whole : options.source;
    if (source.x < 0.0F || source.y < 0.0F || source.getRight() > whole.width || source.getBottom() > whole.height) {
        throw std::invalid_argument("The source of an image shape must lie inside the image.");
    }

    const auto left = static_cast<int>(source.x);
    const auto top = static_cast<int>(source.y);
    const auto width = static_cast<int>(source.width);
    const auto height = static_cast<int>(source.height);
    size = {static_cast<float>(width), static_cast<float>(height)};
    const math::Vec2 center = size * 0.5F;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const math::Color color = image.getPixel(left + x, top + y);
            if (color.a >= options.alphaThreshold) {
                points.push_back(math::Vec2{static_cast<float>(x) + 0.5F, static_cast<float>(y) + 0.5F} - center);
                colors.push_back(color);
            }
        }
    }
    if (points.empty()) {
        throw std::invalid_argument("An image shape needs at least one pixel whose alpha reaches the threshold.");
    }
}

} // namespace haylen::particles2d
