#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace haylen::platform {

// A picture in the YUV 4:2:0 that the image readers of Android hand out, as three planes, each with the distance between its rows and between its samples, and the crop of what shows, which turns into RGBA8 with the coefficients of BT.601 in limited range. Planar pictures have chroma samples next to each other, and semi-planar ones interleave U and V, two bytes apart. It compiles on every platform, so the tests of every host check it.
struct AndroidYuvImage {
    struct Plane {
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;
        int rowStride = 0;
        int pixelStride = 1;
    };

    // The planes of Y, U and V.
    std::array<Plane, 3> planes{};
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;

    // Converts the part that shows into rows of RGBA8 without padding. Throws `std::invalid_argument` for planes that are shorter than the crop needs.
    void toRgba(std::vector<std::uint8_t>& pixels) const;

  private:
    // Whether the last sample that the crop reads lies inside the plane, at the given step of the picture per sample of the plane.
    [[nodiscard]] bool covers(const Plane& plane, int step) const noexcept;
};

} // namespace haylen::platform
