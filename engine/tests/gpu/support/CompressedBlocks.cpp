#include "gpu/support/CompressedBlocks.hpp"

#include <bit>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "gpu/ImageInfo.hpp"
#include "haylen/math/ColorSpace.hpp"

namespace haylen::test {

void CompressedBlocks::BitWriter::write(std::uint32_t value, int count) noexcept {
    for (int bit = 0; bit < count; ++bit) {
        if (((value >> static_cast<unsigned>(bit)) & 1U) != 0U) {
            const auto index = static_cast<std::size_t>(position / 8);
            bytes[index] |= static_cast<std::byte>(1U << static_cast<unsigned>(position % 8));
        }

        ++position;
    }
}

CompressedBlocks::Block CompressedBlocks::make(gpu::Device::PixelFormat format) {
    Block block;
    switch (gpu::ImageInfo::getCompression(format)) {
    case gpu::ImageInfo::Compression::Bc:
        block = makeBc(format);
        break;
    case gpu::ImageInfo::Compression::Etc2:
        block = makeEtc(format);
        break;
    case gpu::ImageInfo::Compression::Astc:
    case gpu::ImageInfo::Compression::AstcHdr:
        block = makeAstc(format);
        break;
    case gpu::ImageInfo::Compression::None:
        break;
    }

    if (isSrgb(format)) {
        block.sampled = {math::ColorSpace::decodeSrgb(block.sampled.r), math::ColorSpace::decodeSrgb(block.sampled.g), math::ColorSpace::decodeSrgb(block.sampled.b), block.sampled.a};
    }

    return block;
}

CompressedBlocks::Block CompressedBlocks::makeBc(gpu::Device::PixelFormat format) {
    using PixelFormat = gpu::Device::PixelFormat;
    Block block{.bytes = {}, .sampled = {0.0F, 0.0F, 0.0F, 1.0F}};
    switch (format) {
    case PixelFormat::Bc1:
    case PixelFormat::Bc1Srgb:
        append(block.bytes, makeColorBlock(block.sampled));
        break;
    case PixelFormat::Bc2:
    case PixelFormat::Bc2Srgb: {
        // Every texel takes the alpha of four bits 9, which is 0.6.
        const std::array<std::byte, 8> alpha{std::byte{0x99}, std::byte{0x99}, std::byte{0x99}, std::byte{0x99}, std::byte{0x99}, std::byte{0x99}, std::byte{0x99}, std::byte{0x99}};
        append(block.bytes, alpha);
        append(block.bytes, makeColorBlock(block.sampled));
        block.sampled.a = 9.0F / 15.0F;
        break;
    }
    case PixelFormat::Bc3:
    case PixelFormat::Bc3Srgb:
        append(block.bytes, makeChannelBlock(153));
        append(block.bytes, makeColorBlock(block.sampled));
        block.sampled.a = 153.0F / 255.0F;
        break;
    case PixelFormat::Bc4:
        append(block.bytes, makeChannelBlock(204));
        block.sampled.r = 204.0F / 255.0F;
        break;
    case PixelFormat::Bc4Snorm:
        append(block.bytes, makeSignedChannelBlock(102));
        block.sampled.r = 102.0F / 127.0F;
        break;
    case PixelFormat::Bc5:
        append(block.bytes, makeChannelBlock(204));
        append(block.bytes, makeChannelBlock(102));
        block.sampled.r = 204.0F / 255.0F;
        block.sampled.g = 102.0F / 255.0F;
        break;
    case PixelFormat::Bc5Snorm:
        append(block.bytes, makeSignedChannelBlock(102));
        append(block.bytes, makeSignedChannelBlock(51));
        block.sampled.r = 102.0F / 127.0F;
        block.sampled.g = 51.0F / 127.0F;
        break;
    case PixelFormat::Bc6hUfloat:
    case PixelFormat::Bc6hFloat:
        append(block.bytes, makeBc6h(format == PixelFormat::Bc6hFloat, block.sampled));
        break;
    default:
        append(block.bytes, makeBc7(block.sampled));
        break;
    }

    return block;
}

CompressedBlocks::Block CompressedBlocks::makeEtc(gpu::Device::PixelFormat format) {
    using PixelFormat = gpu::Device::PixelFormat;
    Block block{.bytes = {}, .sampled = {0.0F, 0.0F, 0.0F, 1.0F}};
    switch (format) {
    case PixelFormat::Etc2Rgb8:
    case PixelFormat::Etc2Rgb8Srgb:
    case PixelFormat::Etc2Rgb8A1:
    case PixelFormat::Etc2Rgb8A1Srgb:
        append(block.bytes, makeEtc2Color(block.sampled));
        break;
    case PixelFormat::Etc2Rgba8:
    case PixelFormat::Etc2Rgba8Srgb:
        append(block.bytes, makeEacAlpha(153));
        append(block.bytes, makeEtc2Color(block.sampled));
        block.sampled.a = 153.0F / 255.0F;
        break;
    case PixelFormat::EacR11:
    case PixelFormat::EacR11Snorm:
        append(block.bytes, makeEac11(format == PixelFormat::EacR11Snorm, 0.8F, block.sampled.r));
        break;
    default: {
        const bool signedChannels = format == PixelFormat::EacRg11Snorm;
        append(block.bytes, makeEac11(signedChannels, 0.8F, block.sampled.r));
        append(block.bytes, makeEac11(signedChannels, 0.4F, block.sampled.g));
        break;
    }
    }

    return block;
}

// A block of one color of ASTC is a void extent block, whose color is four unsigned 16-bit values in the LDR profile and four half floats in the HDR profile.
CompressedBlocks::Block CompressedBlocks::makeAstc(gpu::Device::PixelFormat format) {
    const bool hdr = gpu::ImageInfo::getCompression(format) == gpu::ImageInfo::Compression::AstcHdr;
    Block block{.bytes = {std::byte{0xFC}, std::byte{hdr ? std::uint8_t{0xFF} : std::uint8_t{0xFD}}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}}, .sampled = {}};
    const std::array<float, 4> channels{0.8F, 0.4F, 0.2F, 0.6F};
    std::array<float, 4> sampled{};
    for (std::size_t channel = 0; channel < channels.size(); ++channel) {
        std::uint16_t value = 0;
        if (hdr) {
            value = toHalf(channels[channel]);
            sampled[channel] = fromHalf(value);
        } else {
            value = static_cast<std::uint16_t>(std::lround(channels[channel] * 65535.0F));
            sampled[channel] = static_cast<float>(value >> 8U) / 255.0F;
        }

        block.bytes.push_back(static_cast<std::byte>(value & 0xFFU));
        block.bytes.push_back(static_cast<std::byte>(value >> 8U));
    }

    block.sampled = {sampled[0], sampled[1], sampled[2], sampled[3]};
    return block;
}

// Both colors of the block are the color in five, six and five bits, so every index takes it, opaque.
std::array<std::byte, 8> CompressedBlocks::makeColorBlock(math::Color& sampled) {
    const int red = static_cast<int>(std::lround(0.8F * 31.0F));
    const int green = static_cast<int>(std::lround(0.4F * 63.0F));
    const int blue = static_cast<int>(std::lround(0.2F * 31.0F));
    const auto packed = static_cast<std::uint16_t>((red << 11) | (green << 5) | blue);
    sampled.r = static_cast<float>(expand(red, 5)) / 255.0F;
    sampled.g = static_cast<float>(expand(green, 6)) / 255.0F;
    sampled.b = static_cast<float>(expand(blue, 5)) / 255.0F;
    const auto low = static_cast<std::byte>(packed & 0xFFU);
    const auto high = static_cast<std::byte>(packed >> 8U);
    return {low, high, low, high, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
}

std::array<std::byte, 8> CompressedBlocks::makeChannelBlock(std::uint8_t value) {
    return {std::byte{value}, std::byte{value}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
}

std::array<std::byte, 8> CompressedBlocks::makeSignedChannelBlock(std::int8_t value) {
    const auto bits = std::bit_cast<std::uint8_t>(value);
    return makeChannelBlock(bits);
}

// Mode 11 of BC6H holds one region with endpoints of ten bits, and equal endpoints give every texel the endpoint. Each channel takes the endpoint whose half float lies nearest the color.
std::array<std::byte, 16> CompressedBlocks::makeBc6h(bool signedFloat, math::Color& sampled) {
    const std::array<float, 3> targets{0.8F, 0.4F, 0.2F};
    std::array<int, 3> endpoints{};
    std::array<float, 3> values{};
    for (std::size_t channel = 0; channel < targets.size(); ++channel) {
        float nearest = std::numeric_limits<float>::max();
        const int largest = signedFloat ? 510 : 1023;
        for (int endpoint = 0; endpoint <= largest; ++endpoint) {
            const float value = fromHalf(static_cast<std::uint16_t>(unquantizeBc6h(endpoint, signedFloat)));
            if (std::abs(value - targets[channel]) < nearest) {
                nearest = std::abs(value - targets[channel]);
                endpoints[channel] = endpoint;
                values[channel] = value;
            }
        }
    }

    BitWriter writer;
    writer.write(3, 2);
    writer.write(0, 3);
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
        for (const int value : endpoints) {
            writer.write(static_cast<std::uint32_t>(value), 10);
        }
    }

    sampled = {values[0], values[1], values[2], 1.0F};
    return writer.getBytes();
}

// The unquantized endpoint of ten bits as the half float that BC6H finishes it into.
int CompressedBlocks::unquantizeBc6h(int value, bool signedFloat) noexcept {
    if (signedFloat) {
        const int unquantized = value == 0 ? 0 : (value >= (1 << 9) - 1 ? 0x7FFF : ((value << 15) + 0x4000) >> 9);
        return (unquantized * 31) >> 5;
    }

    const int unquantized = value == 0 ? 0 : (value == (1 << 10) - 1 ? 0xFFFF : ((value << 16) + 0x8000) >> 10);
    return (unquantized * 31) >> 6;
}

// Mode 6 of BC7 holds one region with endpoints of seven bits and a shared lowest bit, and equal endpoints give every texel the endpoint.
std::array<std::byte, 16> CompressedBlocks::makeBc7(math::Color& sampled) {
    const std::array<int, 4> channels{204 >> 1, 102 >> 1, 51 >> 1, 153 >> 1};
    BitWriter writer;
    writer.write(1U << 6U, 7);
    for (const int channel : channels) {
        writer.write(static_cast<std::uint32_t>(channel), 7);
        writer.write(static_cast<std::uint32_t>(channel), 7);
    }

    writer.write(0, 2);
    sampled = {static_cast<float>(channels[0] << 1) / 255.0F, static_cast<float>(channels[1] << 1) / 255.0F, static_cast<float>(channels[2] << 1) / 255.0F, static_cast<float>(channels[3] << 1) / 255.0F};
    return writer.getBytes();
}

// The differential mode of ETC2 with no difference between its halves, the first table of modifiers and every index 0, which adds 2 to the base color of five bits on each channel.
std::array<std::byte, 8> CompressedBlocks::makeEtc2Color(math::Color& sampled) {
    const int red = static_cast<int>(std::lround(0.8F * 31.0F));
    const int green = static_cast<int>(std::lround(0.4F * 31.0F));
    const int blue = static_cast<int>(std::lround(0.2F * 31.0F));
    sampled.r = static_cast<float>(expand(red, 5) + 2) / 255.0F;
    sampled.g = static_cast<float>(expand(green, 5) + 2) / 255.0F;
    sampled.b = static_cast<float>(expand(blue, 5) + 2) / 255.0F;
    return {static_cast<std::byte>(red << 3), static_cast<std::byte>(green << 3), static_cast<std::byte>(blue << 3), std::byte{0x02}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
}

// An alpha block of EAC with a multiplier of 0 gives every texel its base.
std::array<std::byte, 8> CompressedBlocks::makeEacAlpha(std::uint8_t value) {
    return {std::byte{value}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
}

// A channel of eleven bits of EAC with a multiplier of 0 takes its base times 8 with the first modifier of the first table, -3, and the base plus 4 where it has no sign.
std::array<std::byte, 8> CompressedBlocks::makeEac11(bool signedChannel, float target, float& sampled) {
    if (signedChannel) {
        const int base = static_cast<int>(std::lround((target * 1023.0F + 3.0F) / 8.0F));
        sampled = static_cast<float>(base * 8 - 3) / 1023.0F;
        return makeEacAlpha(std::bit_cast<std::uint8_t>(static_cast<std::int8_t>(base)));
    }

    const int base = static_cast<int>(std::lround((target * 2047.0F - 1.0F) / 8.0F));
    sampled = static_cast<float>(base * 8 + 1) / 2047.0F;
    return makeEacAlpha(static_cast<std::uint8_t>(base));
}

float CompressedBlocks::fromHalf(std::uint16_t half) noexcept {
    const int exponent = (half >> 10U) & 0x1F;
    const auto fraction = static_cast<float>(half & 0x3FFU);
    const float magnitude = exponent == 0 ? std::ldexp(fraction, -24) : std::ldexp(1.0F + fraction / 1024.0F, exponent - 15);
    return (half & 0x8000U) != 0U ? -magnitude : magnitude;
}

std::uint16_t CompressedBlocks::toHalf(float value) noexcept {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const int exponent = static_cast<int>((bits >> 23U) & 0xFFU) - 127 + 15;
    const std::uint32_t fraction = (bits >> 13U) & 0x3FFU;
    return static_cast<std::uint16_t>((static_cast<std::uint32_t>(exponent) << 10U) | fraction);
}

int CompressedBlocks::expand(int value, int bits) noexcept {
    return (value << (8 - bits)) | (value >> (2 * bits - 8));
}

bool CompressedBlocks::isSrgb(gpu::Device::PixelFormat format) noexcept {
    switch (format) {
    case gpu::Device::PixelFormat::Bc1Srgb:
    case gpu::Device::PixelFormat::Bc2Srgb:
    case gpu::Device::PixelFormat::Bc3Srgb:
    case gpu::Device::PixelFormat::Bc7Srgb:
    case gpu::Device::PixelFormat::Etc2Rgb8Srgb:
    case gpu::Device::PixelFormat::Etc2Rgb8A1Srgb:
    case gpu::Device::PixelFormat::Etc2Rgba8Srgb:
    case gpu::Device::PixelFormat::Astc4x4Srgb:
    case gpu::Device::PixelFormat::Astc5x4Srgb:
    case gpu::Device::PixelFormat::Astc5x5Srgb:
    case gpu::Device::PixelFormat::Astc6x5Srgb:
    case gpu::Device::PixelFormat::Astc6x6Srgb:
    case gpu::Device::PixelFormat::Astc8x5Srgb:
    case gpu::Device::PixelFormat::Astc8x6Srgb:
    case gpu::Device::PixelFormat::Astc8x8Srgb:
    case gpu::Device::PixelFormat::Astc10x5Srgb:
    case gpu::Device::PixelFormat::Astc10x6Srgb:
    case gpu::Device::PixelFormat::Astc10x8Srgb:
    case gpu::Device::PixelFormat::Astc10x10Srgb:
    case gpu::Device::PixelFormat::Astc12x10Srgb:
    case gpu::Device::PixelFormat::Astc12x12Srgb:
        return true;
    default:
        return false;
    }
}

void CompressedBlocks::append(std::vector<std::byte>& bytes, std::span<const std::byte> block) {
    bytes.insert(bytes.end(), block.begin(), block.end());
}

} // namespace haylen::test
