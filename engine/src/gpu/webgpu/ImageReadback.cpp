#include "gpu/webgpu/ImageReadback.hpp"

#include <span>
#include <stdexcept>

#include "gpu/ImageInfo.hpp"
#include "gpu/PixelConverter.hpp"
#include "gpu/webgpu/Conversion.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu::webgpu {

ImageReadback::ImageReadback(WGPUDevice device, WGPUQueue queue, WGPUTexture texture, Device::PixelFormat pixelFormat, int pixelsWide, int pixelsHigh) : format(pixelFormat), width(pixelsWide), height(pixelsHigh) {
    if (ImageInfo::isDepth(format)) {
        throw ContractError(ContractError::Rule::Readback);
    }

    const auto packedRow = static_cast<std::uint32_t>(ImageInfo::getRowBytes(format, width));
    rowBytes = (packedRow + kRowAlignment - 1U) / kRowAlignment * kRowAlignment;
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-readback");
    descriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    descriptor.size = static_cast<std::uint64_t>(rowBytes) * static_cast<std::uint64_t>(height);
    buffer = wgpuDeviceCreateBuffer(device, &descriptor);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUTexelCopyTextureInfo from = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    from.texture = texture;
    WGPUTexelCopyBufferInfo to = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    to.buffer = buffer;
    to.layout.bytesPerRow = rowBytes;
    to.layout.rowsPerImage = static_cast<std::uint32_t>(height);
    const WGPUExtent3D extent{.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height), .depthOrArrayLayers = 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &from, &to, &extent);
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &commands);
    wgpuCommandBufferRelease(commands);
    wgpuCommandEncoderRelease(encoder);

    WGPUBufferMapCallbackInfo callback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.callback = &ImageReadback::receiveMapping;
    callback.userdata1 = &state;
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, static_cast<std::size_t>(descriptor.size), callback);
}

ImageReadback::~ImageReadback() {
    wgpuBufferDestroy(buffer);
    wgpuBufferRelease(buffer);
}

void ImageReadback::receiveMapping(WGPUMapAsyncStatus status, WGPUStringView, void* first, void*) {
    *static_cast<State*>(first) = status == WGPUMapAsyncStatus_Success ? State::Mapped : State::Failed;
}

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    switch (state) {
    case State::Mapping:
    case State::Delivered:
        return std::nullopt;
    case State::Failed:
        throw std::runtime_error("The GPU could not hand over the pixels of a render target.");
    case State::Mapped:
        break;
    }

    const std::size_t size = static_cast<std::size_t>(rowBytes) * static_cast<std::size_t>(height);
    const auto* mapped = static_cast<const std::byte*>(wgpuBufferGetConstMappedRange(buffer, 0, size));
    const auto targetRow = static_cast<std::size_t>(width) * 4U;
    std::vector<std::uint8_t> pixels(targetRow * static_cast<std::size_t>(height));
    for (int row = 0; row < height; ++row) {
        const std::span<const std::byte> source(mapped + static_cast<std::size_t>(row) * rowBytes, rowBytes);
        PixelConverter::convertRow(format, source, std::span<std::uint8_t>(pixels).subspan(static_cast<std::size_t>(row) * targetRow, targetRow));
    }

    wgpuBufferUnmap(buffer);
    state = State::Delivered;
    return pixels;
}

} // namespace haylen::gpu::webgpu
