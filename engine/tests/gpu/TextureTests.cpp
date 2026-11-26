#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "gpu/ImageInfo.hpp"
#include "gpu/support/CompressedBlocks.hpp"
#include "gpu/support/GpuFixture.hpp"
#include "haylen/math/ColorSpace.hpp"
#include "support/ContractRule.hpp"

namespace haylen::gpu {

class GpuTextureTest : public test::GpuFixture {
  protected:
    static constexpr math::Color kColor{0.8F, 0.4F, 0.2F, 0.6F};
    static constexpr std::array<Device::PixelFormat, 13> kColorFormats{
        Device::PixelFormat::R8, Device::PixelFormat::Rg8, Device::PixelFormat::Rgba8, Device::PixelFormat::Rgba8Srgb, Device::PixelFormat::Bgra8, Device::PixelFormat::Rgb10A2, Device::PixelFormat::Rg11B10Float, Device::PixelFormat::R16Float, Device::PixelFormat::Rg16Float, Device::PixelFormat::Rgba16Float, Device::PixelFormat::R32Float, Device::PixelFormat::Rg32Float, Device::PixelFormat::Rgba32Float,
    };

    // The program that samples the format: the one that filters where the device filters the format, and the one that reads texels without filtering elsewhere.
    [[nodiscard]] std::string_view samplingProgram(Device::PixelFormat format) const noexcept {
        return capabilities().getFormat(format).filter ? "textured" : "unfilterable";
    }

    // The color of the tests as a format holds it, with the alpha of two bits of `Rgb10A2` at its largest and the missing alpha of `Rg11B10Float` as 1.
    [[nodiscard]] static math::Color storedColor(Device::PixelFormat format) noexcept {
        const bool opaque = format == Device::PixelFormat::Rgb10A2 || format == Device::PixelFormat::Rg11B10Float;
        return opaque ? math::Color{kColor.r, kColor.g, kColor.b, 1.0F} : kColor;
    }

    // Whether the format holds an alpha channel, without which its texels read an alpha of 1.
    [[nodiscard]] static bool hasAlpha(Device::PixelFormat format) noexcept {
        switch (format) {
        case Device::PixelFormat::R8:
        case Device::PixelFormat::Rg8:
        case Device::PixelFormat::Rg11B10Float:
        case Device::PixelFormat::R16Float:
        case Device::PixelFormat::Rg16Float:
        case Device::PixelFormat::R32Float:
        case Device::PixelFormat::Rg32Float:
            return false;
        default:
            return true;
        }
    }

    // What a draw samples from a texel of the color: the channels the format lacks read as 0 and its missing alpha as 1.
    [[nodiscard]] static math::Color sampledColor(Device::PixelFormat format) noexcept {
        const math::Color stored = storedColor(format);
        switch (format) {
        case Device::PixelFormat::R8:
        case Device::PixelFormat::R16Float:
        case Device::PixelFormat::R32Float:
            return {stored.r, 0.0F, 0.0F, 1.0F};
        case Device::PixelFormat::Rg8:
        case Device::PixelFormat::Rg16Float:
        case Device::PixelFormat::Rg32Float:
            return {stored.r, stored.g, 0.0F, 1.0F};
        default:
            return stored;
        }
    }

    [[nodiscard]] static std::uint16_t toHalf(float value) noexcept {
        const auto bits = std::bit_cast<std::uint32_t>(value);
        const std::uint32_t sign = (bits >> 16U) & 0x8000U;
        const int exponent = static_cast<int>((bits >> 23U) & 0xFFU) - 127 + 15;
        const std::uint32_t mantissa = (bits >> 13U) & 0x3FFU;

        if (exponent <= 0) {
            return static_cast<std::uint16_t>(sign);
        }

        return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent) << 10U) | mantissa);
    }

    [[nodiscard]] static std::uint8_t toByte(float value) noexcept {
        return static_cast<std::uint8_t>(std::lround(value * 255.0F));
    }

    // The bytes of one texel of the color in the format.
    [[nodiscard]] static std::vector<std::byte> encode(Device::PixelFormat format) {
        const math::Color color = storedColor(format);
        const std::array<float, 4> channels{color.r, color.g, color.b, color.a};
        std::vector<std::byte> texel(ImageInfo::getRowBytes(format, 1));
        const auto put = [&texel](const auto& value, std::size_t offset) { std::memcpy(texel.data() + offset, &value, sizeof(value)); };
        switch (format) {
        case Device::PixelFormat::R8:
        case Device::PixelFormat::Rg8:
        case Device::PixelFormat::Rgba8:
        case Device::PixelFormat::Rgba8Srgb:
            for (std::size_t channel = 0; channel < texel.size(); ++channel) {
                put(toByte(channels[channel]), channel);
            }

            break;
        case Device::PixelFormat::Bgra8:
            put(std::array<std::uint8_t, 4>{toByte(color.b), toByte(color.g), toByte(color.r), toByte(color.a)}, 0);
            break;
        case Device::PixelFormat::Rgb10A2:
            put(static_cast<std::uint32_t>(std::lround(color.r * 1023.0F)) | (static_cast<std::uint32_t>(std::lround(color.g * 1023.0F)) << 10U) | (static_cast<std::uint32_t>(std::lround(color.b * 1023.0F)) << 20U) | (3U << 30U), 0);
            break;
        case Device::PixelFormat::Rg11B10Float:
            put(static_cast<std::uint32_t>(toHalf(color.r) >> 4U) | (static_cast<std::uint32_t>(toHalf(color.g) >> 4U) << 11U) | (static_cast<std::uint32_t>(toHalf(color.b) >> 5U) << 22U), 0);
            break;
        case Device::PixelFormat::R16Float:
        case Device::PixelFormat::Rg16Float:
        case Device::PixelFormat::Rgba16Float:
            for (std::size_t channel = 0; channel < texel.size() / 2; ++channel) {
                put(toHalf(channels[channel]), channel * 2);
            }

            break;
        default:
            for (std::size_t channel = 0; channel < texel.size() / 4; ++channel) {
                put(channels[channel], channel * 4);
            }

            break;
        }
        return texel;
    }

    // Draws the texture into a target of the size with the program and the sampler, and reads it back.
    [[nodiscard]] Pixels sample(std::string_view name, Device::Image texture, Device::Sampler sampler, std::array<float, 4> lookup, int width = 1, int height = 1) {
        const Device::Image target = makeTarget(width, height);
        const std::array<Device::ColorTarget, 1> formats{};
        const Device::Pipeline pipeline = device().makePipeline({.program = program(name), .colors = formats, .label = "haylen-test-sample"});
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = math::Color::black()}}};
        device().beginPass({.colors = colors});
        device().applyPipeline(pipeline);
        Device::Bindings bindings;
        bindings.textures[0] = texture;
        bindings.samplers[0] = sampler;
        device().applyBindings(bindings);
        device().applyUniforms(0, std::as_bytes(std::span(lookup)));
        device().draw(0, 3, 1);
        device().endPass();
        device().commit();
        const Pixels pixels = read(target, width, height);
        device().destroy(pipeline);
        return pixels;
    }

    // A flat texture of the rows of texels, the top row first, with one span per mip level.
    [[nodiscard]] Device::Image makeTexture(int width, int height, std::span<const std::span<const std::byte>> levels, Device::PixelFormat format = Device::PixelFormat::Rgba8, Device::TextureType type = Device::TextureType::Flat, int layers = 1) {
        return makeImage({.type = type, .width = width, .height = height, .layers = layers, .mipLevels = static_cast<int>(levels.size()), .format = format, .levels = levels, .label = "haylen-test-texture"});
    }

    // A texture of 64 by 64 pixels whose every mip level is one color, with a red of 32 for each level below the first, so the red that a draw samples tells the level of detail it read.
    [[nodiscard]] Device::Image makeLevelTexture() {
        std::vector<std::vector<std::byte>> pixels;
        for (int level = 0; level < 7; ++level) {
            const int side = 64 >> level;
            const math::Color color{static_cast<float>(level * 32) / 255.0F, 0.0F, 0.0F, 1.0F};
            const std::array<math::Color, 1> colors{color};
            pixels.push_back(fill(colors, side * side));
        }

        std::vector<std::span<const std::byte>> levels(pixels.begin(), pixels.end());
        return makeTexture(64, 64, levels);
    }

    [[nodiscard]] Device::Sampler makeSampler(const Device::SamplerDesc& desc) {
        const Device::Sampler sampler = device().makeSampler(desc);
        samplers.push_back(sampler);
        return sampler;
    }

    // The texels of layers of one color each, the first layer first.
    [[nodiscard]] static std::vector<std::byte> fill(std::span<const math::Color> colors, int texelsPerLayer) {
        std::vector<std::byte> bytes;
        for (const math::Color color : colors) {
            for (int texel = 0; texel < texelsPerLayer; ++texel) {
                for (const float channel : {color.r, color.g, color.b, color.a}) {
                    bytes.push_back(static_cast<std::byte>(toByte(channel)));
                }
            }
        }

        return bytes;
    }

    void TearDown() override {
        for (const Device::Sampler sampler : samplers) {
            device().destroy(sampler);
        }

        GpuFixture::TearDown();
    }

    static constexpr Device::SamplerDesc kNearest{.minification = Device::Filter::Nearest, .magnification = Device::Filter::Nearest};

    std::vector<Device::Sampler> samplers;
};

// Every color format that the device samples reads back the color its texels hold, with the channels it lacks at their defaults.
TEST_P(GpuTextureTest, SamplesEveryFormatTheDeviceSupports) {
    ASSERT_TRUE(capabilities().getFormat(Device::PixelFormat::Rgba8).sample) << "Every device samples the format Rgba8, which the capabilities must mark.";
    const Device::Sampler sampler = makeSampler(kNearest);
    for (const Device::PixelFormat format : kColorFormats) {
        if (!capabilities().getFormat(format).sample) {
            continue;
        }

        const std::vector<std::byte> texel = encode(format);
        const std::array<std::span<const std::byte>, 1> levels{texel};
        const Device::Image texture = makeTexture(1, 1, levels, format);
        math::Color expected = sampledColor(format);
        if (format == Device::PixelFormat::Rgba8Srgb) {
            expected = {math::ColorSpace::decodeSrgb(expected.r), math::ColorSpace::decodeSrgb(expected.g), math::ColorSpace::decodeSrgb(expected.b), expected.a};
        }

        const Pixels pixels = sample(samplingProgram(format), texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F});
        EXPECT_TRUE(isNear(pixels.at(0, 0), expected, 3)) << static_cast<int>(format);
    }
}

// Every block compressed format that the device samples, from the BC, ETC2, EAC and ASTC families, reads back the color of a block that decodes to one color, uploaded as the blocks it is, and the sRGB formats read it in linear values.
TEST_P(GpuTextureTest, SamplesEveryCompressedFormatTheDeviceSupports) {
    const Device::Sampler sampler = makeSampler(kNearest);
    int sampled = 0;
    for (std::size_t index = 0; index < Device::kPixelFormatCount; ++index) {
        const auto format = static_cast<Device::PixelFormat>(index);
        if (!ImageInfo::isCompressed(format) || !capabilities().getFormat(format).sample) {
            continue;
        }

        const test::CompressedBlocks::Block block = test::CompressedBlocks::make(format);
        const ImageInfo::Block size = ImageInfo::getBlock(format);
        const std::array<std::span<const std::byte>, 1> levels{block.bytes};
        const Device::Image texture = makeTexture(size.width, size.height, levels, format);
        const Pixels pixels = sample("textured", texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F});
        EXPECT_TRUE(isNear(pixels.at(0, 0), block.sampled, 3)) << Device::kPixelFormatNames[index].first;
        ++sampled;
    }

    if (sampled == 0) {
        GTEST_SKIP() << "The GPU of this device samples no block compressed format.";
    }
}

// The rules of the contract refuse a compressed format as an attachment, as a volume, with sides that hold no whole blocks, or on a device that does not sample it.
TEST_P(GpuTextureTest, RefusesCompressedImagesOutsideTheContract) {
    const std::vector<std::byte> blocks(16 * 4);
    const std::array<std::span<const std::byte>, 1> levels{blocks};
    for (std::size_t index = 0; index < Device::kPixelFormatCount; ++index) {
        const auto format = static_cast<Device::PixelFormat>(index);
        if (!ImageInfo::isCompressed(format)) {
            continue;
        }

        const ImageInfo::Block block = ImageInfo::getBlock(format);
        EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeImage({.width = block.width, .height = block.height, .format = format, .kind = Device::ImageKind::RenderTarget}); }), gpu::ContractError::Rule::Image) << index;
        EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeImage({.type = Device::TextureType::Volume, .width = block.width, .height = block.height, .format = format, .levels = levels}); }), gpu::ContractError::Rule::Image) << index;
        EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeImage({.width = block.width + 1, .height = block.height, .format = format, .levels = levels}); }), gpu::ContractError::Rule::Image) << index;
        if (!capabilities().getFormat(format).sample) {
            const std::span<const std::byte> one(blocks.data(), ImageInfo::getRegionBytes(format, block.width, block.height));
            const std::array<std::span<const std::byte>, 1> level{one};
            EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeImage({.width = block.width, .height = block.height, .format = format, .levels = level}); }), gpu::ContractError::Rule::Image) << index;
        }
    }
}

// Every color format that the device marks filterable blends its texels with a linear filter. A row of an empty texel and a texel of the color, drawn four pixels wide, takes three quarters of the color at its third pixel, while the channels the format lacks keep their defaults.
TEST_P(GpuTextureTest, FiltersEveryFormatTheDeviceFilters) {
    ASSERT_TRUE(capabilities().getFormat(Device::PixelFormat::Rgba8).filter) << "Every device filters the format Rgba8, which the capabilities must mark.";
    const Device::Sampler sampler = makeSampler({});
    for (const Device::PixelFormat format : kColorFormats) {
        if (!capabilities().getFormat(format).filter) {
            continue;
        }

        std::vector<std::byte> texels(ImageInfo::getRowBytes(format, 1));
        const std::vector<std::byte> texel = encode(format);
        texels.insert(texels.end(), texel.begin(), texel.end());
        const std::array<std::span<const std::byte>, 1> levels{texels};
        const Device::Image texture = makeTexture(2, 1, levels, format);

        math::Color full = sampledColor(format);
        if (format == Device::PixelFormat::Rgba8Srgb) {
            full = {math::ColorSpace::decodeSrgb(full.r), math::ColorSpace::decodeSrgb(full.g), math::ColorSpace::decodeSrgb(full.b), full.a};
        }

        const math::Color empty = hasAlpha(format) ? math::Color::transparent() : math::Color{0.0F, 0.0F, 0.0F, 1.0F};
        const math::Color expected{empty.r + (full.r - empty.r) * 0.75F, empty.g + (full.g - empty.g) * 0.75F, empty.b + (full.b - empty.b) * 0.75F, empty.a + (full.a - empty.a) * 0.75F};
        const Pixels pixels = sample("textured", texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F}, 4, 1);
        EXPECT_TRUE(isNear(pixels.at(2, 0), expected, 3)) << static_cast<int>(format);
    }
}

// Every color format that the device renders and samples takes the clear of a pass, which a draw samples back.
TEST_P(GpuTextureTest, RendersIntoEveryFormatTheDeviceRenders) {
    ASSERT_TRUE(capabilities().getFormat(Device::PixelFormat::Rgba8).render) << "Every device renders into the format Rgba8, which the capabilities must mark.";
    const Device::Sampler sampler = makeSampler(kNearest);
    for (const Device::PixelFormat format : kColorFormats) {
        const auto index = static_cast<std::size_t>(format);
        if (!capabilities().formats[index].render || !capabilities().formats[index].sample) {
            continue;
        }

        const Device::Image target = makeTarget(1, 1, format);
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = storedColor(format)}}};
        device().beginPass({.colors = colors});
        device().endPass();
        const Pixels pixels = sample(samplingProgram(format), target, sampler, {0.0F, 0.0F, 0.0F, 1.0F});
        EXPECT_TRUE(isNear(pixels.at(0, 0), sampledColor(format), 3)) << static_cast<int>(format);
    }
}

// Every color format that the device renders reads back as RGBA8, with the channels it lacks as 0 and its missing alpha as 1, also the float formats of OpenGL ES and WebGL2. The bytes of an sRGB target hold its encoded values, which the test leaves out.
TEST_P(GpuTextureTest, ReadsBackEveryFormatTheDeviceRenders) {
    for (const Device::PixelFormat format : kColorFormats) {
        const auto index = static_cast<std::size_t>(format);
        if (!capabilities().formats[index].render || format == Device::PixelFormat::Rgba8Srgb) {
            continue;
        }

        const Device::Image target = makeTarget(1, 1, format);
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = storedColor(format)}}};
        device().beginPass({.colors = colors});
        device().endPass();
        device().commit();
        EXPECT_TRUE(isNear(read(target, 1, 1).at(0, 0), sampledColor(format), 3)) << static_cast<int>(format);
    }
}

// Every color format that takes more samples than one draws with them and resolves into a target of one sample, which a draw samples back, so a count that the device reports is a count it draws with.
TEST_P(GpuTextureTest, ResolvesEveryFormatWithTheSampleCountsTheDeviceReports) {
    const Device::Sampler sampler = makeSampler(kNearest);
    for (const Device::PixelFormat format : kColorFormats) {
        const auto index = static_cast<std::size_t>(format);
        if (!capabilities().formats[index].sample) {
            continue;
        }

        for (const int samples : {2, 4, 8}) {
            if ((capabilities().formats[index].sampleCounts & static_cast<unsigned>(samples)) == 0) {
                continue;
            }

            const Device::Image multisampled = makeTarget(1, 1, format, samples);
            const Device::Image resolved = makeTarget(1, 1, format);
            const std::array<Device::ColorAttachment, 1> colors{{{.image = multisampled, .store = Device::Store::Discard, .clear = storedColor(format), .resolve = resolved}}};
            device().beginPass({.colors = colors});
            device().endPass();
            const Pixels pixels = sample(samplingProgram(format), resolved, sampler, {0.0F, 0.0F, 0.0F, 1.0F});
            EXPECT_TRUE(isNear(pixels.at(0, 0), sampledColor(format), 3)) << static_cast<int>(format) << " with " << samples << " samples";
        }
    }
}

// An image takes only the sizes and the samples that the capabilities give its type and its format, which every device checks before it makes the image.
TEST_P(GpuTextureTest, RefusesImagesBeyondTheCapabilities) {
    const Device::Capabilities& limits = capabilities();
    const int volume = limits.maxVolumeSize + 1;
    const int cube = limits.maxCubeSize + 1;
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeImage({.type = Device::TextureType::Volume, .width = volume, .height = 1, .layers = 1, .kind = Device::ImageKind::RenderTarget}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeImage({.type = Device::TextureType::Volume, .width = 1, .height = 1, .layers = volume, .kind = Device::ImageKind::RenderTarget}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeImage({.type = Device::TextureType::Cube, .width = cube, .height = cube, .layers = 6, .kind = Device::ImageKind::RenderTarget}); }), gpu::ContractError::Rule::Image);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeImage({.type = Device::TextureType::Cube, .width = 4, .height = 8, .layers = 6, .kind = Device::ImageKind::RenderTarget}); }), gpu::ContractError::Rule::Image);

    for (std::size_t index = 0; index < limits.formats.size(); ++index) {
        const auto format = static_cast<Device::PixelFormat>(index);
        const Device::ImageKind kind = ImageInfo::isDepth(format) ? Device::ImageKind::DepthStencil : Device::ImageKind::RenderTarget;
        for (const int samples : {2, 4, 8}) {
            if (!limits.formats[index].render || (limits.formats[index].sampleCounts & static_cast<unsigned>(samples)) != 0) {
                continue;
            }

            EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeImage({.width = 4, .height = 4, .sampleCount = samples, .format = format, .kind = kind}); }), gpu::ContractError::Rule::Image) << index << " with " << samples << " samples";
        }
    }
}

// Arrays, volumes and cubes read the layer, the slice and the face that a draw asks for.
TEST_P(GpuTextureTest, SamplesArraysVolumesAndCubes) {
    const Device::Sampler sampler = makeSampler(kNearest);
    const std::array<math::Color, 6> colors{{{1.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}, {1.0F, 1.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 1.0F, 1.0F}, {1.0F, 0.0F, 1.0F, 1.0F}}};

    const std::vector<std::byte> layerTexels = fill(std::span(colors).first(3), 4);
    const std::array<std::span<const std::byte>, 1> layerLevels{layerTexels};
    const Device::Image layers = makeTexture(2, 2, layerLevels, Device::PixelFormat::Rgba8, Device::TextureType::Array, 3);
    for (int layer = 0; layer < 3; ++layer) {
        EXPECT_TRUE(isNear(sample("array", layers, sampler, {0.0F, static_cast<float>(layer), 0.0F, 1.0F}).at(0, 0), colors[static_cast<std::size_t>(layer)])) << "Layer " << layer;
    }

    const std::vector<std::byte> sliceTexels = fill(std::span(colors).first(4), 4);
    const std::array<std::span<const std::byte>, 1> sliceLevels{sliceTexels};
    const Device::Image volume = makeTexture(2, 2, sliceLevels, Device::PixelFormat::Rgba8, Device::TextureType::Volume, 4);
    for (int slice = 0; slice < 4; ++slice) {
        EXPECT_TRUE(isNear(sample("volume", volume, sampler, {0.0F, (static_cast<float>(slice) + 0.5F) / 4.0F, 0.0F, 1.0F}).at(0, 0), colors[static_cast<std::size_t>(slice)])) << "Slice " << slice;
    }

    const std::vector<std::byte> faceTexels = fill(colors, 4);
    const std::array<std::span<const std::byte>, 1> faceLevels{faceTexels};
    const Device::Image cube = makeTexture(2, 2, faceLevels, Device::PixelFormat::Rgba8, Device::TextureType::Cube, 6);
    for (int face = 0; face < 6; ++face) {
        EXPECT_TRUE(isNear(sample("cube", cube, sampler, {0.0F, static_cast<float>(face), 0.0F, 1.0F}).at(0, 0), colors[static_cast<std::size_t>(face)])) << "Face " << face;
    }
}

// Each mip level holds its own pixels, which a draw reads by its level, and the levels of detail of a sampler clamp the level it reads.
TEST_P(GpuTextureTest, ReadsEveryMipLevelWithinTheLevelsOfTheSampler) {
    const std::array<math::Color, 3> colors{{{1.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}}};
    const std::vector<std::byte> first = fill(std::span(colors).first(1), 16);
    const std::vector<std::byte> second = fill(std::span(colors).subspan(1, 1), 4);
    const std::vector<std::byte> third = fill(std::span(colors).subspan(2, 1), 1);
    const std::array<std::span<const std::byte>, 3> levels{first, second, third};
    const Device::Image texture = makeTexture(4, 4, levels);

    const Device::Sampler sampler = makeSampler(kNearest);
    for (int level = 0; level < 3; ++level) {
        EXPECT_TRUE(isNear(sample("textured", texture, sampler, {static_cast<float>(level), 0.0F, 0.0F, 1.0F}).at(0, 0), colors[static_cast<std::size_t>(level)])) << "Level " << level;
    }

    const Device::Sampler clamped = makeSampler({.minification = Device::Filter::Nearest, .magnification = Device::Filter::Nearest, .minLod = 1.0F, .maxLod = 1.0F});
    EXPECT_TRUE(isNear(sample("textured", texture, clamped, {0.0F, 0.0F, 0.0F, 1.0F}).at(0, 0), colors[1]));
    EXPECT_TRUE(isNear(sample("textured", texture, clamped, {2.0F, 0.0F, 0.0F, 1.0F}).at(0, 0), colors[1]));
}

// Nearest filtering keeps the texels apart and linear filtering mixes them, and the wraps repeat, mirror or clamp the coordinates past the edge.
TEST_P(GpuTextureTest, FiltersAndWrapsAsTheSamplerSays) {
    const std::array<math::Color, 2> colors{math::Color::black(), math::Color::white()};
    const std::vector<std::byte> texels = fill(colors, 1);
    const std::array<std::span<const std::byte>, 1> levels{texels};
    const Device::Image texture = makeTexture(2, 1, levels);

    const Pixels nearest = sample("textured", texture, makeSampler(kNearest), {0.0F, 0.0F, 0.0F, 1.0F}, 4, 1);
    const Pixels linear = sample("textured", texture, makeSampler({}), {0.0F, 0.0F, 0.0F, 1.0F}, 4, 1);
    EXPECT_TRUE(isNear(nearest.at(1, 0), math::Color::black()));
    EXPECT_TRUE(isNear(nearest.at(2, 0), math::Color::white()));
    EXPECT_TRUE(isNear(linear.at(1, 0), {0.25F, 0.25F, 0.25F, 1.0F}, 3));
    EXPECT_TRUE(isNear(linear.at(2, 0), {0.75F, 0.75F, 0.75F, 1.0F}, 3));

    const std::array<std::pair<Device::Wrap, std::array<math::Color, 2>>, 3> wraps{{
        {Device::Wrap::Repeat, {math::Color::black(), math::Color::white()}},
        {Device::Wrap::MirroredRepeat, {math::Color::white(), math::Color::black()}},
        {Device::Wrap::ClampToEdge, {math::Color::white(), math::Color::white()}},
    }};

    for (const auto& [wrap, expected] : wraps) {
        const Device::Sampler sampler = makeSampler({.minification = Device::Filter::Nearest, .magnification = Device::Filter::Nearest, .wrapU = wrap, .wrapV = wrap});
        const Pixels pixels = sample("textured", texture, sampler, {0.0F, 0.0F, 0.0F, 2.0F}, 8, 1);
        EXPECT_TRUE(isNear(pixels.at(5, 0), expected[0])) << static_cast<int>(wrap);
        EXPECT_TRUE(isNear(pixels.at(6, 0), expected[1])) << static_cast<int>(wrap);
    }

    const Device::Sampler anisotropic = makeSampler({.mipmap = Device::Filter::Linear, .maxAnisotropy = std::min(4, capabilities().maxAnisotropy)});
    EXPECT_TRUE(isNear(sample("textured", texture, anisotropic, {0.0F, 0.0F, 0.0F, 1.0F}, 4, 1).at(0, 0), math::Color::black()));
}

// A pixel that covers 16 texels across and one down reads a sharper level with anisotropic filtering than without it, which reads the level of the longer side.
TEST_P(GpuTextureTest, ReadsSharperLevelsWithAnisotropicFiltering) {
    if (capabilities().maxAnisotropy < 16) {
        GTEST_SKIP() << "The GPU of this device filters with at most " << capabilities().maxAnisotropy << " samples of anisotropy.";
    }

    const Device::Image texture = makeLevelTexture();
    const Device::SamplerDesc linear{.mipmap = Device::Filter::Linear, .wrapU = Device::Wrap::Repeat, .wrapV = Device::Wrap::Repeat};
    Device::SamplerDesc anisotropic = linear;
    anisotropic.maxAnisotropy = 16;
    const std::array<float, 4> lookup{0.0F, 1.0F / 16.0F, 0.0F, 1.0F};
    const Pixels plain = sample("implicit", texture, makeSampler(linear), lookup, 4, 4);
    const Pixels sharp = sample("implicit", texture, makeSampler(anisotropic), lookup, 4, 4);
    EXPECT_GT(plain.at(1, 1).r, 0.4F);
    EXPECT_LT(sharp.at(1, 1).r, plain.at(1, 1).r - 0.2F);
}

// A bias of the level of detail moves a lookup to a smaller level, where the device takes one.
TEST_P(GpuTextureTest, BiasesTheLevelOfDetailOfALookup) {
    if (capabilities().maxLodBias < 2.0F) {
        GTEST_SKIP() << "The samplers of this graphics API take no bias of the level of detail of 2.";
    }

    const Device::Image texture = makeLevelTexture();
    const Device::SamplerDesc unbiased{.mipmap = Device::Filter::Linear, .wrapU = Device::Wrap::Repeat, .wrapV = Device::Wrap::Repeat};
    Device::SamplerDesc biased = unbiased;
    biased.lodBias = 2.0F;
    const std::array<float, 4> lookup{0.0F, 0.25F, 0.0F, 0.25F};
    const Pixels plain = sample("implicit", texture, makeSampler(unbiased), lookup, 4, 4);
    const Pixels moved = sample("implicit", texture, makeSampler(biased), lookup, 4, 4);
    EXPECT_TRUE(isNear(plain.at(1, 1), {64.0F / 255.0F, 0.0F, 0.0F, 1.0F}, 12));
    EXPECT_TRUE(isNear(moved.at(1, 1), {128.0F / 255.0F, 0.0F, 0.0F, 1.0F}, 12));
}

// A sampler that clamps to its border across reads the border color past the right edge of the image, where the device has border colors.
TEST_P(GpuTextureTest, ReadsTheBorderColorOutsideTheImage) {
    if (!capabilities().borderColors) {
        GTEST_SKIP() << "The samplers of this graphics API read no border color.";
    }

    const std::array<math::Color, 2> red{math::Color{1.0F, 0.0F, 0.0F, 1.0F}, math::Color{1.0F, 0.0F, 0.0F, 1.0F}};
    const std::vector<std::byte> texels = fill(red, 1);
    const std::array<std::span<const std::byte>, 1> levels{texels};
    const Device::Image texture = makeTexture(2, 1, levels);
    for (const auto& [border, expected] : std::array<std::pair<Device::BorderColor, math::Color>, 3>{{
             {Device::BorderColor::OpaqueWhite, math::Color::white()},
             {Device::BorderColor::OpaqueBlack, math::Color::black()},
             {Device::BorderColor::TransparentBlack, math::Color::transparent()},
         }}) {
        const Device::Sampler sampler = makeSampler({.minification = Device::Filter::Nearest, .magnification = Device::Filter::Nearest, .wrapU = Device::Wrap::ClampToBorder, .borderColor = border});
        const Pixels pixels = sample("implicit", texture, sampler, {0.0F, 1.0F, 0.0F, 2.0F}, 8, 1);
        EXPECT_TRUE(isNear(pixels.at(1, 0), math::Color{1.0F, 0.0F, 0.0F, 1.0F})) << static_cast<int>(border);
        EXPECT_TRUE(isNear(pixels.at(6, 0), expected)) << static_cast<int>(border);
    }
}

// Each axis of a sampler wraps as it says, so a texture repeats across while it clamps down.
TEST_P(GpuTextureTest, WrapsEachAxisApart) {
    const std::array<math::Color, 4> colors{math::Color::black(), math::Color::white(), math::Color{1.0F, 0.0F, 0.0F, 1.0F}, math::Color{0.0F, 1.0F, 0.0F, 1.0F}};
    std::vector<std::byte> texels;
    for (const math::Color color : colors) {
        const std::array<math::Color, 1> one{color};
        const std::vector<std::byte> texel = fill(one, 1);
        texels.insert(texels.end(), texel.begin(), texel.end());
    }

    const std::array<std::span<const std::byte>, 1> levels{texels};
    const Device::Image texture = makeTexture(2, 2, levels);
    const Device::Sampler sampler = makeSampler({.minification = Device::Filter::Nearest, .magnification = Device::Filter::Nearest, .wrapU = Device::Wrap::Repeat, .wrapV = Device::Wrap::ClampToEdge});
    const Pixels pixels = sample("implicit", texture, sampler, {0.0F, 2.0F, 0.0F, 2.0F}, 8, 8);
    EXPECT_TRUE(isNear(pixels.at(5, 1), math::Color::black()));
    EXPECT_TRUE(isNear(pixels.at(7, 1), math::Color::white()));
    EXPECT_TRUE(isNear(pixels.at(5, 6), math::Color{1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(7, 6), math::Color{0.0F, 1.0F, 0.0F, 1.0F}));
}

// The rules of samplers refuse anisotropy beyond the device or with a nearest filter, levels of detail out of order, a bias beyond the device and border colors on a device without them.
TEST_P(GpuTextureTest, RefusesSamplersBeyondTheCapabilities) {
    const Device::Capabilities& limits = capabilities();
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeSampler({.mipmap = Device::Filter::Linear, .maxAnisotropy = limits.maxAnisotropy + 1}); }), gpu::ContractError::Rule::Sampler);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeSampler({.minification = Device::Filter::Nearest, .mipmap = Device::Filter::Linear, .maxAnisotropy = 2}); }), gpu::ContractError::Rule::Sampler);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeSampler({.minLod = 2.0F, .maxLod = 1.0F}); }), gpu::ContractError::Rule::Sampler);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeSampler({.lodBias = limits.maxLodBias + 1.0F}); }), gpu::ContractError::Rule::Sampler);
    if (!limits.borderColors) {
        EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeSampler({.wrapU = Device::Wrap::ClampToBorder}); }), gpu::ContractError::Rule::Sampler);
    }
}

// A dynamic image takes new pixels every frame, which the draws of that frame sample.
TEST_P(GpuTextureTest, SamplesTheNewPixelsOfDynamicImagesEachFrame) {
    const Device::Image texture = makeImage({.width = 2, .height = 2, .kind = Device::ImageKind::Dynamic, .label = "haylen-test-dynamic"});
    const Device::Sampler sampler = makeSampler(kNearest);
    for (const math::Color color : {math::Color{1.0F, 0.0F, 0.0F, 1.0F}, math::Color{0.0F, 1.0F, 0.0F, 1.0F}, math::Color{0.0F, 0.0F, 1.0F, 1.0F}}) {
        const std::vector<std::byte> texels = fill(std::span(&color, 1), 4);
        const std::array<Device::ImageWrite, 1> writes{{{.image = texture, .region = {0, 0, 2, 2}, .pixels = texels}}};
        device().writeImages(writes);
        EXPECT_TRUE(isNear(sample("textured", texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F}).at(0, 0), color)) << color.toHex();
    }
}

// A write of a region of a dynamic image replaces the pixels of the region and keeps the others, the way a glyph atlas sends only the glyphs it added.
TEST_P(GpuTextureTest, KeepsThePixelsOutsideTheRegionThatAWriteReplaces) {
    const Device::Image texture = makeImage({.width = 4, .height = 1, .kind = Device::ImageKind::Dynamic, .label = "haylen-test-dynamic"});
    const Device::Sampler sampler = makeSampler(kNearest);
    const math::Color red{1.0F, 0.0F, 0.0F, 1.0F};
    const math::Color green{0.0F, 1.0F, 0.0F, 1.0F};
    const std::vector<std::byte> whole = fill(std::span(&red, 1), 4);
    const std::array<Device::ImageWrite, 1> first{{{.image = texture, .region = {0, 0, 4, 1}, .pixels = whole}}};
    device().writeImages(first);
    EXPECT_TRUE(isNear(sample("textured", texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F}, 4, 1).at(3, 0), red));

    const std::vector<std::byte> part = fill(std::span(&green, 1), 2);
    const std::array<Device::ImageWrite, 1> second{{{.image = texture, .region = {2, 0, 2, 1}, .pixels = part}}};
    device().writeImages(second);
    const Pixels pixels = sample("textured", texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F}, 4, 1);
    EXPECT_TRUE(isNear(pixels.at(0, 0), red));
    EXPECT_TRUE(isNear(pixels.at(1, 0), red));
    EXPECT_TRUE(isNear(pixels.at(2, 0), green));
    EXPECT_TRUE(isNear(pixels.at(3, 0), green));
}

// Only a dynamic image takes new pixels, so a write into a render target is refused.
TEST_P(GpuTextureTest, RefusesNewPixelsForImagesThatAreNotDynamic) {
    const Device::Image target = makeTarget(2, 2);
    const std::array<std::byte, 16> texels{};
    const std::array<Device::ImageWrite, 1> writes{{{.image = target, .region = {0, 0, 2, 2}, .pixels = texels}}};
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().writeImages(writes); }), gpu::ContractError::Rule::ImageWrite);
}

// Every color format that the device samples reads back the color that a write of a dynamic image of the format gave it, which reaches the image the way the glyph atlases of text do.
TEST_P(GpuTextureTest, SamplesEveryFormatThatDynamicImagesWrite) {
    const Device::Sampler sampler = makeSampler(kNearest);
    for (const Device::PixelFormat format : kColorFormats) {
        if (!capabilities().getFormat(format).sample) {
            continue;
        }

        const std::vector<std::byte> texel = encode(format);
        std::vector<std::byte> texels;
        for (int index = 0; index < 64; ++index) {
            texels.insert(texels.end(), texel.begin(), texel.end());
        }

        const Device::Image texture = makeImage({.width = 8, .height = 8, .format = format, .kind = Device::ImageKind::Dynamic, .label = "haylen-test-dynamic"});
        const std::array<Device::ImageWrite, 1> writes{{{.image = texture, .region = {0, 0, 8, 8}, .pixels = texels}}};
        device().writeImages(writes);
        math::Color expected = sampledColor(format);
        if (format == Device::PixelFormat::Rgba8Srgb) {
            expected = {math::ColorSpace::decodeSrgb(expected.r), math::ColorSpace::decodeSrgb(expected.g), math::ColorSpace::decodeSrgb(expected.b), expected.a};
        }

        const Pixels pixels = sample(samplingProgram(format), texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F});
        EXPECT_TRUE(isNear(pixels.at(0, 0), expected, 3)) << static_cast<int>(format);
    }
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuTextureTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuTextureTest);

} // namespace haylen::gpu
