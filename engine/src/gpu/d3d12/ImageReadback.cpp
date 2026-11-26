#include "gpu/d3d12/ImageReadback.hpp"

#include <cstddef>
#include <span>
#include <utility>

#include "gpu/ImageInfo.hpp"
#include "gpu/PixelConverter.hpp"

namespace haylen::gpu::d3d12 {

ImageReadback::ImageReadback(ReadbackCopy copy, UINT pitch, Device::PixelFormat pixelFormat, int imageWidth, int imageHeight) : readback(std::move(copy)), rowPitch(pitch), format(pixelFormat), width(imageWidth), height(imageHeight) {}

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    if (delivered) {
        return std::nullopt;
    }

    const std::optional<std::span<const std::byte>> mapped = readback.map(static_cast<std::size_t>(rowPitch) * static_cast<std::size_t>(height), "The GPU could not hand over the pixels of a render target.");
    if (!mapped) {
        return std::nullopt;
    }

    const std::size_t rowBytes = ImageInfo::getRowBytes(format, width);
    const std::size_t pixelRow = static_cast<std::size_t>(width) * 4;
    std::vector<std::uint8_t> pixels(pixelRow * static_cast<std::size_t>(height));
    for (int row = 0; row < height; ++row) {
        const std::span<const std::byte> source = mapped->subspan(static_cast<std::size_t>(row) * rowPitch, rowBytes);
        PixelConverter::convertRow(format, source, std::span(pixels).subspan(static_cast<std::size_t>(row) * pixelRow, pixelRow));
    }

    readback.unmap();
    delivered = true;
    return pixels;
}

} // namespace haylen::gpu::d3d12
