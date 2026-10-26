#include "gpu/d3d12/ImageReadback.hpp"

#include <stdexcept>
#include <utility>

#include "gpu/direct3d/Formats.hpp"
#include "gpu/direct3d/PixelConverter.hpp"

namespace haylen::gpu::d3d12 {

ImageReadback::ImageReadback(Microsoft::WRL::ComPtr<ID3D12Resource> copy, Microsoft::WRL::ComPtr<ID3D12Fence> frames, std::uint64_t frameValue, UINT pitch, Device::PixelFormat pixelFormat, int imageWidth, int imageHeight) : buffer(std::move(copy)), fence(std::move(frames)), value(frameValue), rowPitch(pitch), format(pixelFormat), width(imageWidth), height(imageHeight) {}

// A removed device reports every fence value as passed, and its buffers then refuse to map.
std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    if (fence->GetCompletedValue() < value) {
        return std::nullopt;
    }

    const D3D12_RANGE range{.Begin = 0, .End = static_cast<SIZE_T>(rowPitch) * static_cast<SIZE_T>(height)};
    void* mapped = nullptr;
    if (FAILED(buffer->Map(0, &range, &mapped))) {
        throw std::runtime_error("The GPU could not hand over the pixels of a render target.");
    }

    const std::size_t rowBytes = static_cast<std::size_t>(width) * direct3d::Formats::getBytesPerPixel(format);
    const std::size_t pixelRow = static_cast<std::size_t>(width) * 4;
    std::vector<std::uint8_t> pixels(pixelRow * static_cast<std::size_t>(height));
    for (int row = 0; row < height; ++row) {
        const auto* source = static_cast<const std::byte*>(mapped) + static_cast<std::size_t>(row) * rowPitch;
        direct3d::PixelConverter::convertRow(format, {source, rowBytes}, std::span(pixels).subspan(static_cast<std::size_t>(row) * pixelRow, pixelRow));
    }
    const D3D12_RANGE nothing{.Begin = 0, .End = 0};
    buffer->Unmap(0, &nothing);
    return pixels;
}

} // namespace haylen::gpu::d3d12
