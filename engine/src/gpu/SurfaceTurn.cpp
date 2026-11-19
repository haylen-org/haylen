#include "haylen/gpu/SurfaceTurn.hpp"

namespace haylen::gpu {

// A clockwise quarter turn takes the pixel at `(x, y)` of a window of height `H` to `(H - y, x)` of the images, which are as wide as the window is tall.
math::Rect SurfaceTurn::turnPixels(const math::Rect& pixels, math::Vec2 window, int turns) noexcept {
    switch (turns & 3) {
    case 1:
        return {window.y - pixels.y - pixels.height, pixels.x, pixels.height, pixels.width};
    case 2:
        return {window.x - pixels.x - pixels.width, window.y - pixels.y - pixels.height, pixels.width, pixels.height};
    case 3:
        return {pixels.y, window.x - pixels.x - pixels.width, pixels.height, pixels.width};
    default:
        return pixels;
    }
}

// Clip space points up, so the clockwise quarter turn of a pixel takes the position `(x, y)` in clip space to `(y, -x)`.
SurfaceTurn::Matrix SurfaceTurn::turnClip(const Matrix& matrix, int turns) noexcept {
    Matrix turned = matrix;
    for (std::size_t column = 0; column < 4; ++column) {
        const float x = matrix[column * 4];
        const float y = matrix[column * 4 + 1];
        switch (turns & 3) {
        case 1:
            turned[column * 4] = y;
            turned[column * 4 + 1] = -x;
            break;
        case 2:
            turned[column * 4] = -x;
            turned[column * 4 + 1] = -y;
            break;
        case 3:
            turned[column * 4] = -y;
            turned[column * 4 + 1] = x;
            break;
        default:
            break;
        }
    }

    return turned;
}

} // namespace haylen::gpu
