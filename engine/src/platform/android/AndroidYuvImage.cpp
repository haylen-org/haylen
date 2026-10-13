#include "platform/android/AndroidYuvImage.hpp"

#include <algorithm>
#include <stdexcept>

namespace haylen::platform {

bool AndroidYuvImage::covers(const Plane& plane, int step) const noexcept {
    if (plane.data == nullptr || plane.rowStride <= 0 || plane.pixelStride <= 0) {
        return false;
    }
    const auto lastRow = static_cast<std::size_t>((top + height - 1) / step);
    const auto lastColumn = static_cast<std::size_t>((left + width - 1) / step);
    return lastRow * static_cast<std::size_t>(plane.rowStride) + lastColumn * static_cast<std::size_t>(plane.pixelStride) < plane.size;
}

// The fixed-point coefficients of BT.601 in limited range scale by 256, so every pixel converts with integers.
void AndroidYuvImage::toRgba(std::vector<std::uint8_t>& pixels) const {
    if (width <= 0 || height <= 0 || left < 0 || top < 0 || !covers(planes[0], 1) || !covers(planes[1], 2) || !covers(planes[2], 2)) {
        throw std::invalid_argument("The picture of the video decoder is smaller than its size says.");
    }
    const Plane& luma = planes[0];
    const Plane& blue = planes[1];
    const Plane& red = planes[2];
    pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    std::uint8_t* target = pixels.data();
    for (int row = top; row < top + height; ++row) {
        const std::uint8_t* lumaRow = luma.data + static_cast<std::size_t>(row) * static_cast<std::size_t>(luma.rowStride);
        const std::uint8_t* blueRow = blue.data + static_cast<std::size_t>(row / 2) * static_cast<std::size_t>(blue.rowStride);
        const std::uint8_t* redRow = red.data + static_cast<std::size_t>(row / 2) * static_cast<std::size_t>(red.rowStride);
        for (int column = left; column < left + width; ++column, target += 4) {
            const int c = 298 * (lumaRow[static_cast<std::size_t>(column) * static_cast<std::size_t>(luma.pixelStride)] - 16);
            const int d = blueRow[static_cast<std::size_t>(column / 2) * static_cast<std::size_t>(blue.pixelStride)] - 128;
            const int e = redRow[static_cast<std::size_t>(column / 2) * static_cast<std::size_t>(red.pixelStride)] - 128;
            target[0] = static_cast<std::uint8_t>(std::clamp((c + 409 * e + 128) >> 8, 0, 255));
            target[1] = static_cast<std::uint8_t>(std::clamp((c - 100 * d - 208 * e + 128) >> 8, 0, 255));
            target[2] = static_cast<std::uint8_t>(std::clamp((c + 516 * d + 128) >> 8, 0, 255));
            target[3] = 255;
        }
    }
}

} // namespace haylen::platform
