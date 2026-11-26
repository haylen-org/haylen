#include "gpu/d3d11/ImageReadback.hpp"

#include <cstddef>
#include <span>
#include <utility>

#include "gpu/ImageInfo.hpp"
#include "gpu/PixelConverter.hpp"

namespace haylen::gpu::d3d11 {

ImageReadback::ImageReadback(StagingCopy copy, Device::PixelFormat pixelFormat, int imageWidth, int imageHeight) : staging(std::move(copy)), format(pixelFormat), width(imageWidth), height(imageHeight) {}

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    if (delivered) {
        return std::nullopt;
    }

    const std::optional<D3D11_MAPPED_SUBRESOURCE> mapped = staging.map("The GPU could not hand over the pixels of a render target.");
    if (!mapped) {
        return std::nullopt;
    }

    const std::size_t rowBytes = ImageInfo::getRowBytes(format, width);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    for (int row = 0; row < height; ++row) {
        const auto* source = static_cast<const std::byte*>(mapped->pData) + static_cast<std::size_t>(row) * mapped->RowPitch;
        PixelConverter::convertRow(format, {source, rowBytes}, std::span(pixels).subspan(static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * 4, static_cast<std::size_t>(width) * 4));
    }

    staging.unmap();
    delivered = true;
    return pixels;
}

} // namespace haylen::gpu::d3d11
