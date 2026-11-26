#include "gpu/webgpu/ImageReadback.hpp"

#include <cstddef>
#include <span>

#include "gpu/ImageInfo.hpp"
#include "gpu/PixelConverter.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu::webgpu {

ImageReadback::ImageReadback(WGPUDevice device, WGPUQueue queue, WGPUTexture texture, Device::PixelFormat pixelFormat, int pixelsWide, int pixelsHigh) : format(pixelFormat), width(pixelsWide), height(pixelsHigh), rowBytes(measureRow(pixelFormat, pixelsWide)), copy(device, static_cast<std::uint64_t>(rowBytes) * static_cast<std::uint64_t>(pixelsHigh), "haylen-readback") {
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUTexelCopyTextureInfo from = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    from.texture = texture;
    WGPUTexelCopyBufferInfo to = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    to.buffer = copy.getBuffer();
    to.layout.bytesPerRow = rowBytes;
    to.layout.rowsPerImage = static_cast<std::uint32_t>(height);
    const WGPUExtent3D extent{.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height), .depthOrArrayLayers = 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &from, &to, &extent);
    copy.submit(queue, encoder);
}

std::uint32_t ImageReadback::measureRow(Device::PixelFormat format, int width) {
    if (ImageInfo::isDepth(format)) {
        throw ContractError(ContractError::Rule::Readback);
    }

    const auto packedRow = static_cast<std::uint32_t>(ImageInfo::getRowBytes(format, width));
    return (packedRow + kRowAlignment - 1U) / kRowAlignment * kRowAlignment;
}

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    const std::optional<std::span<const std::byte>> mapped = copy.getMapped("The GPU could not hand over the pixels of a render target.");
    if (!mapped) {
        return std::nullopt;
    }

    const auto targetRow = static_cast<std::size_t>(width) * 4U;
    std::vector<std::uint8_t> pixels(targetRow * static_cast<std::size_t>(height));
    for (int row = 0; row < height; ++row) {
        const std::span<const std::byte> source = mapped->subspan(static_cast<std::size_t>(row) * rowBytes, rowBytes);
        PixelConverter::convertRow(format, source, std::span<std::uint8_t>(pixels).subspan(static_cast<std::size_t>(row) * targetRow, targetRow));
    }

    copy.deliver();
    return pixels;
}

} // namespace haylen::gpu::webgpu
