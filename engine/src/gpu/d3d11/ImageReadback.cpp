#include "gpu/d3d11/ImageReadback.hpp"

#include <stdexcept>
#include <utility>

#include "gpu/direct3d/Formats.hpp"
#include "gpu/direct3d/PixelConverter.hpp"

namespace haylen::gpu::d3d11 {

ImageReadback::ImageReadback(Microsoft::WRL::ComPtr<ID3D11DeviceContext> owner, Microsoft::WRL::ComPtr<ID3D11Texture2D> copy, Microsoft::WRL::ComPtr<ID3D11Query> finished, Device::PixelFormat pixelFormat, int imageWidth, int imageHeight) : context(std::move(owner)), staging(std::move(copy)), done(std::move(finished)), format(pixelFormat), width(imageWidth), height(imageHeight) {}

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    if (context->GetData(done.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE) {
        return std::nullopt;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT result = context->Map(staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) {
        return std::nullopt;
    }
    if (FAILED(result)) {
        throw std::runtime_error("The GPU could not hand over the pixels of a render target.");
    }

    const std::size_t rowBytes = static_cast<std::size_t>(width) * direct3d::Formats::getBytesPerPixel(format);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    for (int row = 0; row < height; ++row) {
        const auto* source = static_cast<const std::byte*>(mapped.pData) + static_cast<std::size_t>(row) * mapped.RowPitch;
        direct3d::PixelConverter::convertRow(format, {source, rowBytes}, std::span(pixels).subspan(static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * 4, static_cast<std::size_t>(width) * 4));
    }
    context->Unmap(staging.Get(), 0);
    return pixels;
}

} // namespace haylen::gpu::d3d11
