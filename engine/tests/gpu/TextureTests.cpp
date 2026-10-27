#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

#include "gpu/ImageInfo.hpp"
#include "gpu/support/GpuFixture.hpp"

namespace haylen::gpu {

class GpuTextureTest : public test::GpuFixture {
  protected:
    static constexpr math::Color kColor{0.8F, 0.4F, 0.2F, 0.6F};
    static constexpr std::array<Device::PixelFormat, 12> kColorFormats{
        Device::PixelFormat::R8, Device::PixelFormat::Rg8, Device::PixelFormat::Rgba8, Device::PixelFormat::Rgba8Srgb, Device::PixelFormat::Bgra8, Device::PixelFormat::Rgb10A2, Device::PixelFormat::R16Float, Device::PixelFormat::Rg16Float, Device::PixelFormat::Rgba16Float, Device::PixelFormat::R32Float, Device::PixelFormat::Rg32Float, Device::PixelFormat::Rgba32Float,
    };

    // The program that samples the format: the one that filters where the device filters the format, and the one that reads texels without filtering elsewhere.
    [[nodiscard]] std::string_view samplingProgram(Device::PixelFormat format) const noexcept {
        return capabilities().filterableFormats[static_cast<std::size_t>(format)] ? "textured" : "unfilterable";
    }

    // The color of the tests as a format holds it, with the alpha of two bits of `Rgb10A2` at its largest.
    [[nodiscard]] static math::Color storedColor(Device::PixelFormat format) noexcept {
        return format == Device::PixelFormat::Rgb10A2 ? math::Color{kColor.r, kColor.g, kColor.b, 1.0F} : kColor;
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

    [[nodiscard]] static float toLinear(float value) noexcept {
        return value <= 0.04045F ? value / 12.92F : std::pow((value + 0.055F) / 1.055F, 2.4F);
    }

    // The bytes of one texel of the color in the format.
    [[nodiscard]] static std::vector<std::byte> encode(Device::PixelFormat format) {
        const math::Color color = storedColor(format);
        const std::array<float, 4> channels{color.r, color.g, color.b, color.a};
        std::vector<std::byte> texel(ImageInfo::getBytesPerPixel(format));
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
    ASSERT_TRUE(capabilities().textureFormats[static_cast<std::size_t>(Device::PixelFormat::Rgba8)]) << "Every device samples the format Rgba8, which the capabilities must mark.";
    const Device::Sampler sampler = makeSampler(kNearest);
    for (const Device::PixelFormat format : kColorFormats) {
        if (!capabilities().textureFormats[static_cast<std::size_t>(format)]) {
            continue;
        }
        const std::vector<std::byte> texel = encode(format);
        const std::array<std::span<const std::byte>, 1> levels{texel};
        const Device::Image texture = makeTexture(1, 1, levels, format);
        math::Color expected = sampledColor(format);
        if (format == Device::PixelFormat::Rgba8Srgb) {
            expected = {toLinear(expected.r), toLinear(expected.g), toLinear(expected.b), expected.a};
        }
        const Pixels pixels = sample(samplingProgram(format), texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F});
        EXPECT_TRUE(isNear(pixels.at(0, 0), expected, 3)) << static_cast<int>(format);
    }
}

// Every color format that the device marks filterable blends its texels with a linear filter. A row of an empty texel and a texel of the color, drawn four pixels wide, takes three quarters of the color at its third pixel, while the channels the format lacks keep their defaults.
TEST_P(GpuTextureTest, FiltersEveryFormatTheDeviceFilters) {
    ASSERT_TRUE(capabilities().filterableFormats[static_cast<std::size_t>(Device::PixelFormat::Rgba8)]) << "Every device filters the format Rgba8, which the capabilities must mark.";
    const Device::Sampler sampler = makeSampler({});
    for (const Device::PixelFormat format : kColorFormats) {
        if (!capabilities().filterableFormats[static_cast<std::size_t>(format)]) {
            continue;
        }
        std::vector<std::byte> texels(ImageInfo::getBytesPerPixel(format));
        const std::vector<std::byte> texel = encode(format);
        texels.insert(texels.end(), texel.begin(), texel.end());
        const std::array<std::span<const std::byte>, 1> levels{texels};
        const Device::Image texture = makeTexture(2, 1, levels, format);

        math::Color full = sampledColor(format);
        if (format == Device::PixelFormat::Rgba8Srgb) {
            full = {toLinear(full.r), toLinear(full.g), toLinear(full.b), full.a};
        }
        const math::Color empty = sampledColor(format) == storedColor(format) ? math::Color::transparent() : math::Color{0.0F, 0.0F, 0.0F, 1.0F};
        const math::Color expected{empty.r + (full.r - empty.r) * 0.75F, empty.g + (full.g - empty.g) * 0.75F, empty.b + (full.b - empty.b) * 0.75F, empty.a + (full.a - empty.a) * 0.75F};
        const Pixels pixels = sample("textured", texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F}, 4, 1);
        EXPECT_TRUE(isNear(pixels.at(2, 0), expected, 3)) << static_cast<int>(format);
    }
}

// Every color format that the device renders and samples takes the clear of a pass, which a draw samples back.
TEST_P(GpuTextureTest, RendersIntoEveryFormatTheDeviceRenders) {
    ASSERT_TRUE(capabilities().renderFormats[static_cast<std::size_t>(Device::PixelFormat::Rgba8)]) << "Every device renders into the format Rgba8, which the capabilities must mark.";
    const Device::Sampler sampler = makeSampler(kNearest);
    for (const Device::PixelFormat format : kColorFormats) {
        const auto index = static_cast<std::size_t>(format);
        if (!capabilities().renderFormats[index] || !capabilities().textureFormats[index]) {
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

    const Device::Sampler anisotropic = makeSampler({.mipmap = Device::Filter::Linear, .maxAnisotropy = 4});
    EXPECT_TRUE(isNear(sample("textured", texture, anisotropic, {0.0F, 0.0F, 0.0F, 1.0F}, 4, 1).at(0, 0), math::Color::black()));
}

// A dynamic image takes new pixels every frame, which the draws of that frame sample.
TEST_P(GpuTextureTest, SamplesTheNewPixelsOfDynamicImagesEachFrame) {
    const Device::Image texture = makeImage({.width = 2, .height = 2, .kind = Device::ImageKind::Dynamic, .label = "haylen-test-dynamic"});
    const Device::Sampler sampler = makeSampler(kNearest);
    for (const math::Color color : {math::Color{1.0F, 0.0F, 0.0F, 1.0F}, math::Color{0.0F, 1.0F, 0.0F, 1.0F}, math::Color{0.0F, 0.0F, 1.0F, 1.0F}}) {
        const std::vector<std::byte> texels = fill(std::span(&color, 1), 4);
        const std::array<Device::ImageWrite, 1> writes{{{.image = texture, .pixels = texels}}};
        device().writeImages(writes);
        EXPECT_TRUE(isNear(sample("textured", texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F}).at(0, 0), color)) << color.toHex();
    }
}

// Every color format that the device samples reads back the color that a write of a dynamic image of the format gave it, which reaches the image the way the glyph atlases of text do.
TEST_P(GpuTextureTest, SamplesEveryFormatThatDynamicImagesWrite) {
    const Device::Sampler sampler = makeSampler(kNearest);
    for (const Device::PixelFormat format : kColorFormats) {
        if (!capabilities().textureFormats[static_cast<std::size_t>(format)]) {
            continue;
        }
        const std::vector<std::byte> texel = encode(format);
        std::vector<std::byte> texels;
        for (int index = 0; index < 64; ++index) {
            texels.insert(texels.end(), texel.begin(), texel.end());
        }
        const Device::Image texture = makeImage({.width = 8, .height = 8, .format = format, .kind = Device::ImageKind::Dynamic, .label = "haylen-test-dynamic"});
        const std::array<Device::ImageWrite, 1> writes{{{.image = texture, .pixels = texels}}};
        device().writeImages(writes);
        math::Color expected = sampledColor(format);
        if (format == Device::PixelFormat::Rgba8Srgb) {
            expected = {toLinear(expected.r), toLinear(expected.g), toLinear(expected.b), expected.a};
        }
        const Pixels pixels = sample(samplingProgram(format), texture, sampler, {0.0F, 0.0F, 0.0F, 1.0F});
        EXPECT_TRUE(isNear(pixels.at(0, 0), expected, 3)) << static_cast<int>(format);
    }
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuTextureTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuTextureTest);

} // namespace haylen::gpu
