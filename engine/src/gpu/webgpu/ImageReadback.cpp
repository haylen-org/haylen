#include "gpu/webgpu/ImageReadback.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "gpu/ImageInfo.hpp"
#include "gpu/webgpu/Conversion.hpp"

namespace haylen::gpu::webgpu {

ImageReadback::ImageReadback(WGPUDevice device, WGPUQueue queue, WGPUTexture texture, Device::PixelFormat pixelFormat, int pixelsWide, int pixelsHigh) : format(pixelFormat), width(pixelsWide), height(pixelsHigh) {
    if (ImageInfo::isDepth(format)) {
        throw std::logic_error("Only the pixels of color render targets can be read back.");
    }

    const auto bytesPerPixel = static_cast<std::uint32_t>(ImageInfo::getBytesPerPixel(format));
    rowBytes = (static_cast<std::uint32_t>(width) * bytesPerPixel + kRowAlignment - 1U) / kRowAlignment * kRowAlignment;
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
        convertRow(source, std::span<std::uint8_t>(pixels).subspan(static_cast<std::size_t>(row) * targetRow, targetRow));
    }
    wgpuBufferUnmap(buffer);
    return pixels;
}

float ImageReadback::fromHalf(std::uint16_t bits) noexcept {
    const std::uint32_t sign = (bits & 0x8000U) << 16U;
    const std::uint32_t exponent = (bits >> 10U) & 0x1FU;
    const std::uint32_t mantissa = bits & 0x3FFU;
    if (exponent == 0) {
        const float value = std::ldexp(static_cast<float>(mantissa), -24);
        return sign != 0 ? -value : value;
    }
    if (exponent == 31) {
        return std::bit_cast<float>(sign | 0x7F800000U | (mantissa << 13U));
    }
    return std::bit_cast<float>(sign | ((exponent + 112U) << 23U) | (mantissa << 13U));
}

std::uint8_t ImageReadback::toByte(float value) noexcept {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

std::uint8_t ImageReadback::toByte(std::uint32_t value, std::uint32_t maximum) noexcept {
    return static_cast<std::uint8_t>((value * 255U + maximum / 2U) / maximum);
}

void ImageReadback::convertRow(std::span<const std::byte> source, std::span<std::uint8_t> target) const noexcept {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(source.data());
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width); ++pixel) {
        std::uint8_t* out = target.data() + pixel * 4U;
        std::array<float, 4> channels{0.0F, 0.0F, 0.0F, 1.0F};
        switch (format) {
        case Device::PixelFormat::Rgba8:
        case Device::PixelFormat::Rgba8Srgb:
            std::memcpy(out, bytes + pixel * 4U, 4U);
            continue;
        case Device::PixelFormat::Bgra8:
            out[0] = bytes[pixel * 4U + 2U];
            out[1] = bytes[pixel * 4U + 1U];
            out[2] = bytes[pixel * 4U];
            out[3] = bytes[pixel * 4U + 3U];
            continue;
        case Device::PixelFormat::R8:
            out[0] = bytes[pixel];
            out[1] = 0;
            out[2] = 0;
            out[3] = 255;
            continue;
        case Device::PixelFormat::Rg8:
            out[0] = bytes[pixel * 2U];
            out[1] = bytes[pixel * 2U + 1U];
            out[2] = 0;
            out[3] = 255;
            continue;
        case Device::PixelFormat::Rgb10A2: {
            std::uint32_t packed = 0;
            std::memcpy(&packed, bytes + pixel * 4U, 4U);
            out[0] = toByte(packed & 0x3FFU, 0x3FFU);
            out[1] = toByte((packed >> 10U) & 0x3FFU, 0x3FFU);
            out[2] = toByte((packed >> 20U) & 0x3FFU, 0x3FFU);
            out[3] = toByte(packed >> 30U, 3U);
            continue;
        }
        case Device::PixelFormat::R16Float:
        case Device::PixelFormat::Rg16Float:
        case Device::PixelFormat::Rgba16Float: {
            const std::size_t count = format == Device::PixelFormat::R16Float ? 1U : (format == Device::PixelFormat::Rg16Float ? 2U : 4U);
            for (std::size_t channel = 0; channel < count; ++channel) {
                std::uint16_t half = 0;
                std::memcpy(&half, bytes + (pixel * count + channel) * 2U, 2U);
                channels[channel] = fromHalf(half);
            }
            break;
        }
        case Device::PixelFormat::R32Float:
        case Device::PixelFormat::Rg32Float:
        case Device::PixelFormat::Rgba32Float: {
            const std::size_t count = format == Device::PixelFormat::R32Float ? 1U : (format == Device::PixelFormat::Rg32Float ? 2U : 4U);
            std::memcpy(channels.data(), bytes + pixel * count * 4U, count * 4U);
            break;
        }
        case Device::PixelFormat::Depth16:
        case Device::PixelFormat::Depth32Float:
        case Device::PixelFormat::Depth24Stencil8:
        case Device::PixelFormat::Depth32FloatStencil8:
            break;
        }
        for (std::size_t channel = 0; channel < 4U; ++channel) {
            out[channel] = toByte(channels[channel]);
        }
    }
}

} // namespace haylen::gpu::webgpu
