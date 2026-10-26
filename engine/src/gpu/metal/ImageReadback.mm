#include "gpu/metal/ImageReadback.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "gpu/metal/Formats.hpp"

namespace haylen::gpu::metal {

ImageReadback::ImageReadback(id<MTLCommandQueue> queue, id<MTLTexture> texture, gpu::Device::PixelFormat pixelFormat, int width, int height) : format(pixelFormat), pixelCount(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
    const NSUInteger rowBytes = static_cast<NSUInteger>(width) * Formats::getBytesPerPixel(format);
    const NSUInteger imageBytes = rowBytes * static_cast<NSUInteger>(height);
    buffer = [queue.device newBufferWithLength:imageBytes options:MTLResourceStorageModeShared];
    if (buffer == nil) {
        throw std::runtime_error("The GPU has no memory to read the pixels of a render target into.");
    }

    @autoreleasepool {
        const id<MTLCommandBuffer> commands = [queue commandBuffer];
        commands.label = @"haylen-readback";
        const id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
        [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(static_cast<NSUInteger>(width), static_cast<NSUInteger>(height), 1) toBuffer:buffer destinationOffset:0 destinationBytesPerRow:rowBytes destinationBytesPerImage:imageBytes];
        [blit endEncoding];

        // A copy that failed, such as on a lost device, still ends, so the caller never waits forever.
        const std::shared_ptr<std::atomic<bool>> finished = done;
        [commands addCompletedHandler:^(id<MTLCommandBuffer>) { finished->store(true, std::memory_order_release); }];
        [commands commit];
    }
}

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    if (!done->load(std::memory_order_acquire)) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> pixels(pixelCount * 4U);
    const auto* source = static_cast<const std::byte*>(buffer.contents);
    if (format == gpu::Device::PixelFormat::Rgba8 || format == gpu::Device::PixelFormat::Rgba8Srgb) {
        std::memcpy(pixels.data(), source, pixels.size());
        return pixels;
    }

    const std::size_t stride = Formats::getBytesPerPixel(format);
    for (std::size_t index = 0; index < pixelCount; ++index) {
        const std::array<std::uint8_t, 4> pixel = readPixel(source + index * stride);
        std::memcpy(pixels.data() + index * 4U, pixel.data(), pixel.size());
    }
    return pixels;
}

// Half floats of the GPU carry a sign, five bits of exponent and ten of mantissa.
float ImageReadback::fromHalf(std::uint16_t value) noexcept {
    const float sign = (value & 0x8000U) != 0U ? -1.0F : 1.0F;
    const int exponent = (value >> 10U) & 0x1F;
    const auto mantissa = static_cast<float>(value & 0x3FFU);
    if (exponent == 0) {
        return sign * std::ldexp(mantissa, -24);
    }
    if (exponent == 31) {
        return mantissa == 0.0F ? sign * std::numeric_limits<float>::infinity() : std::numeric_limits<float>::quiet_NaN();
    }
    return sign * std::ldexp(mantissa + 1024.0F, exponent - 25);
}

std::array<std::uint8_t, 4> ImageReadback::toBytes(const std::array<float, 4>& color) noexcept {
    std::array<std::uint8_t, 4> bytes{};
    for (std::size_t channel = 0; channel < bytes.size(); ++channel) {
        bytes[channel] = static_cast<std::uint8_t>(std::lround(std::clamp(color[channel], 0.0F, 1.0F) * 255.0F));
    }
    return bytes;
}

std::array<float, 4> ImageReadback::readHalves(const std::byte* source, std::size_t channels) noexcept {
    std::array<std::uint16_t, 4> halves{};
    std::memcpy(halves.data(), source, channels * sizeof(std::uint16_t));
    std::array<float, 4> color{0.0F, 0.0F, 0.0F, 1.0F};
    for (std::size_t channel = 0; channel < channels; ++channel) {
        color[channel] = fromHalf(halves[channel]);
    }
    return color;
}

std::array<float, 4> ImageReadback::readFloats(const std::byte* source, std::size_t channels) noexcept {
    std::array<float, 4> color{0.0F, 0.0F, 0.0F, 1.0F};
    std::memcpy(color.data(), source, channels * sizeof(float));
    return color;
}

// Formats without some channel give it zero, and formats without alpha give an opaque one.
std::array<std::uint8_t, 4> ImageReadback::readPixel(const std::byte* source) const noexcept {
    using PixelFormat = gpu::Device::PixelFormat;
    std::array<std::uint8_t, 4> bytes{};
    std::memcpy(bytes.data(), source, std::min<std::size_t>(Formats::getBytesPerPixel(format), bytes.size()));

    switch (format) {
    case PixelFormat::R8:
        return {bytes[0], 0, 0, 255};
    case PixelFormat::Rg8:
        return {bytes[0], bytes[1], 0, 255};
    case PixelFormat::Bgra8:
        return {bytes[2], bytes[1], bytes[0], bytes[3]};
    case PixelFormat::Rgb10A2: {
        std::uint32_t packed = 0;
        std::memcpy(&packed, source, sizeof(packed));
        return toBytes({static_cast<float>(packed & 0x3FFU) / 1023.0F, static_cast<float>((packed >> 10U) & 0x3FFU) / 1023.0F, static_cast<float>((packed >> 20U) & 0x3FFU) / 1023.0F, static_cast<float>(packed >> 30U) / 3.0F});
    }
    case PixelFormat::R16Float:
        return toBytes(readHalves(source, 1));
    case PixelFormat::Rg16Float:
        return toBytes(readHalves(source, 2));
    case PixelFormat::Rgba16Float:
        return toBytes(readHalves(source, 4));
    case PixelFormat::R32Float:
        return toBytes(readFloats(source, 1));
    case PixelFormat::Rg32Float:
        return toBytes(readFloats(source, 2));
    case PixelFormat::Rgba32Float:
        return toBytes(readFloats(source, 4));
    default:
        return bytes;
    }
}

} // namespace haylen::gpu::metal
