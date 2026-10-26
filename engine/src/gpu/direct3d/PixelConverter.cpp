#include "gpu/direct3d/PixelConverter.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

#include "gpu/direct3d/Formats.hpp"

namespace haylen::gpu::direct3d {

void PixelConverter::convertRow(Device::PixelFormat format, std::span<const std::byte> row, std::span<std::uint8_t> pixels) noexcept {
    const std::size_t size = Formats::getBytesPerPixel(format);
    const std::size_t count = std::min(row.size() / size, pixels.size() / 4);

    // The formats of 8-bit channels in the order of the result copy whole rows.
    if (format == Device::PixelFormat::Rgba8 || format == Device::PixelFormat::Rgba8Srgb) {
        std::memcpy(pixels.data(), row.data(), count * 4);
        return;
    }

    for (std::size_t index = 0; index < count; ++index) {
        const Channels channels = read(format, row.data() + index * size);
        for (std::size_t channel = 0; channel < 4; ++channel) {
            pixels[index * 4 + channel] = toByte(channels[channel]);
        }
    }
}

PixelConverter::Channels PixelConverter::read(Device::PixelFormat format, const std::byte* pixel) noexcept {
    using PixelFormat = Device::PixelFormat;
    const auto byte = [pixel](std::size_t index) { return static_cast<float>(std::to_integer<std::uint8_t>(pixel[index])) / 255.0F; };

    switch (format) {
    case PixelFormat::R8:
        return {byte(0), 0.0F, 0.0F, 1.0F};
    case PixelFormat::Rg8:
        return {byte(0), byte(1), 0.0F, 1.0F};
    case PixelFormat::Bgra8:
        return {byte(2), byte(1), byte(0), byte(3)};
    case PixelFormat::Rgb10A2: {
        std::uint32_t packed = 0;
        std::memcpy(&packed, pixel, sizeof(packed));
        return {static_cast<float>(packed & 0x3FFU) / 1023.0F, static_cast<float>((packed >> 10U) & 0x3FFU) / 1023.0F, static_cast<float>((packed >> 20U) & 0x3FFU) / 1023.0F, static_cast<float>(packed >> 30U) / 3.0F};
    }
    case PixelFormat::R16Float:
        return {readHalf(pixel), 0.0F, 0.0F, 1.0F};
    case PixelFormat::Rg16Float:
        return {readHalf(pixel), readHalf(pixel + 2), 0.0F, 1.0F};
    case PixelFormat::Rgba16Float:
        return {readHalf(pixel), readHalf(pixel + 2), readHalf(pixel + 4), readHalf(pixel + 6)};
    case PixelFormat::R32Float:
        return {readFloat(pixel), 0.0F, 0.0F, 1.0F};
    case PixelFormat::Rg32Float:
        return {readFloat(pixel), readFloat(pixel + 4), 0.0F, 1.0F};
    case PixelFormat::Rgba32Float:
        return {readFloat(pixel), readFloat(pixel + 4), readFloat(pixel + 8), readFloat(pixel + 12)};
    default:
        return {byte(0), byte(1), byte(2), byte(3)};
    }
}

// A half float widens to a float by moving its sign, exponent and fraction into their places, with denormals scaled by hand.
float PixelConverter::readHalf(const std::byte* bytes) noexcept {
    std::uint16_t half = 0;
    std::memcpy(&half, bytes, sizeof(half));
    const std::uint32_t sign = (half & 0x8000U) << 16U;
    const std::uint32_t exponent = (half >> 10U) & 0x1FU;
    const std::uint32_t fraction = half & 0x3FFU;

    if (exponent == 0) {
        const float value = std::ldexp(static_cast<float>(fraction), -24);
        return sign != 0 ? -value : value;
    }
    if (exponent == 0x1FU) {
        return std::bit_cast<float>(sign | 0x7F800000U | (fraction << 13U));
    }
    return std::bit_cast<float>(sign | ((exponent + 112U) << 23U) | (fraction << 13U));
}

float PixelConverter::readFloat(const std::byte* bytes) noexcept {
    float value = 0.0F;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

std::uint8_t PixelConverter::toByte(float value) noexcept {
    if (!(value > 0.0F)) {
        return 0;
    }
    return static_cast<std::uint8_t>(std::lround(std::min(value, 1.0F) * 255.0F));
}

} // namespace haylen::gpu::direct3d
