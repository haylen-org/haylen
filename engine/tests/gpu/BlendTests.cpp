#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <span>

#include "gpu/support/GpuFixture.hpp"

namespace haylen::gpu {

class GpuBlendTest : public test::GpuFixture {
  protected:
    static constexpr math::Color kSource{0.6F, 0.4F, 0.2F, 0.5F};
    static constexpr math::Color kDestination{0.2F, 0.4F, 0.6F, 0.8F};
    static constexpr math::Color kBlendColor{0.25F, 0.5F, 0.75F, 1.0F};
    static constexpr std::array<Device::BlendFactor, 13> kFactors{
        Device::BlendFactor::Zero, Device::BlendFactor::One, Device::BlendFactor::SourceColor, Device::BlendFactor::OneMinusSourceColor, Device::BlendFactor::SourceAlpha, Device::BlendFactor::OneMinusSourceAlpha, Device::BlendFactor::DestinationColor, Device::BlendFactor::OneMinusDestinationColor, Device::BlendFactor::DestinationAlpha, Device::BlendFactor::OneMinusDestinationAlpha, Device::BlendFactor::SourceAlphaSaturated, Device::BlendFactor::BlendColor, Device::BlendFactor::OneMinusBlendColor,
    };

    // The factor of a channel, the alpha at index 3, as every graphics API defines it.
    [[nodiscard]] static float factorOf(Device::BlendFactor factor, std::size_t channel) {
        const std::array<float, 4> source{kSource.r, kSource.g, kSource.b, kSource.a};
        const std::array<float, 4> destination{kDestination.r, kDestination.g, kDestination.b, kDestination.a};
        const std::array<float, 4> constant{kBlendColor.r, kBlendColor.g, kBlendColor.b, kBlendColor.a};
        switch (factor) {
        case Device::BlendFactor::Zero:
            return 0.0F;
        case Device::BlendFactor::One:
            return 1.0F;
        case Device::BlendFactor::SourceColor:
            return source[channel];
        case Device::BlendFactor::OneMinusSourceColor:
            return 1.0F - source[channel];
        case Device::BlendFactor::SourceAlpha:
            return kSource.a;
        case Device::BlendFactor::OneMinusSourceAlpha:
            return 1.0F - kSource.a;
        case Device::BlendFactor::DestinationColor:
            return destination[channel];
        case Device::BlendFactor::OneMinusDestinationColor:
            return 1.0F - destination[channel];
        case Device::BlendFactor::DestinationAlpha:
            return kDestination.a;
        case Device::BlendFactor::OneMinusDestinationAlpha:
            return 1.0F - kDestination.a;
        case Device::BlendFactor::SourceAlphaSaturated:
            return channel == 3 ? 1.0F : std::min(kSource.a, 1.0F - kDestination.a);
        case Device::BlendFactor::BlendColor:
            return constant[channel];
        case Device::BlendFactor::OneMinusBlendColor:
            return 1.0F - constant[channel];
        }
        return 0.0F;
    }

    // Draws the source color over the destination color with the blend into a pixel of its own, and reads it back.
    [[nodiscard]] math::Color blend(const Device::Blend& blend, Device::ColorMask mask = Device::ColorMask::All) {
        const Device::Image target = makeTarget(1, 1);
        const std::array<Device::ColorTarget, 1> formats{{{.writeMask = mask, .blend = blend}}};
        const Device::Pipeline pipeline = makePlanePipeline(formats);
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = kDestination}}};
        device().beginPass({.colors = colors});
        device().applyPipeline(pipeline);
        device().applyBlendColor(kBlendColor);
        drawPlane(kSource);
        device().endPass();
        device().commit();
        return read(target, 1, 1).at(0, 0);
    }
};

// Every factor scales the source when the destination counts for nothing, and the destination when the source counts for nothing.
TEST_P(GpuBlendTest, BlendsWithEveryFactor) {
    const std::array<float, 4> source{kSource.r, kSource.g, kSource.b, kSource.a};
    const std::array<float, 4> destination{kDestination.r, kDestination.g, kDestination.b, kDestination.a};
    for (const Device::BlendFactor factor : kFactors) {
        std::array<float, 4> scaledSource{};
        std::array<float, 4> scaledDestination{};
        for (std::size_t channel = 0; channel < 4; ++channel) {
            scaledSource[channel] = source[channel] * factorOf(factor, channel);
            scaledDestination[channel] = destination[channel] * factorOf(factor, channel);
        }
        const Device::Blend sourceBlend{.enabled = true, .sourceColor = factor, .destinationColor = Device::BlendFactor::Zero, .sourceAlpha = factor, .destinationAlpha = Device::BlendFactor::Zero};
        const Device::Blend destinationBlend{.enabled = true, .sourceColor = Device::BlendFactor::Zero, .destinationColor = factor, .sourceAlpha = Device::BlendFactor::Zero, .destinationAlpha = factor};
        EXPECT_TRUE(isNear(blend(sourceBlend), {scaledSource[0], scaledSource[1], scaledSource[2], scaledSource[3]}, 3)) << "Source factor " << static_cast<int>(factor);
        EXPECT_TRUE(isNear(blend(destinationBlend), {scaledDestination[0], scaledDestination[1], scaledDestination[2], scaledDestination[3]}, 3)) << "Destination factor " << static_cast<int>(factor);
    }
}

// The operations add, subtract either way, and keep the smaller or the larger channel, clamped to the range of the target.
TEST_P(GpuBlendTest, BlendsWithEveryOperation) {
    // clang-format off
    const auto with = [](Device::BlendOperation operation) {
        return Device::Blend{.enabled = true, .sourceColor = Device::BlendFactor::One, .destinationColor = Device::BlendFactor::One, .colorOperation = operation, .sourceAlpha = Device::BlendFactor::One, .destinationAlpha = Device::BlendFactor::One, .alphaOperation = operation};
    };
    // clang-format on
    EXPECT_TRUE(isNear(blend(with(Device::BlendOperation::Add)), {0.8F, 0.8F, 0.8F, 1.0F}, 3));
    EXPECT_TRUE(isNear(blend(with(Device::BlendOperation::Subtract)), {0.4F, 0.0F, 0.0F, 0.0F}, 3));
    EXPECT_TRUE(isNear(blend(with(Device::BlendOperation::ReverseSubtract)), {0.0F, 0.0F, 0.4F, 0.3F}, 3));
    EXPECT_TRUE(isNear(blend(with(Device::BlendOperation::Minimum)), {0.2F, 0.4F, 0.2F, 0.5F}, 3));
    EXPECT_TRUE(isNear(blend(with(Device::BlendOperation::Maximum)), {0.6F, 0.4F, 0.6F, 0.8F}, 3));
}

// A pipeline writes only the channels of its mask, and a mask of none leaves the target as it was.
TEST_P(GpuBlendTest, WritesOnlyTheChannelsOfItsMask) {
    EXPECT_TRUE(isNear(blend({}, Device::ColorMask::None), kDestination));
    EXPECT_TRUE(isNear(blend({}, Device::ColorMask::Rgb), {kSource.r, kSource.g, kSource.b, kDestination.a}));
    const auto redAndAlpha = static_cast<Device::ColorMask>(static_cast<int>(Device::ColorMask::Red) | static_cast<int>(Device::ColorMask::Alpha));
    EXPECT_TRUE(isNear(blend({}, redAndAlpha), {kSource.r, kDestination.g, kDestination.b, kSource.a}));
    const auto greenAndBlue = static_cast<Device::ColorMask>(static_cast<int>(Device::ColorMask::Green) | static_cast<int>(Device::ColorMask::Blue));
    EXPECT_TRUE(isNear(blend({}, greenAndBlue), {kDestination.r, kSource.g, kSource.b, kDestination.a}));
}

// A device with independent blending blends and masks each color target of a pipeline apart.
TEST_P(GpuBlendTest, BlendsEachTargetApartWhereTheDeviceCan) {
    if (!capabilities().independentBlend) {
        GTEST_SKIP() << "The device blends every color target of a pipeline alike.";
    }
    const std::array<Device::Image, 4> targets{makeTarget(1, 1), makeTarget(1, 1), makeTarget(1, 1), makeTarget(1, 1)};
    const Device::Blend adding{.enabled = true, .sourceColor = Device::BlendFactor::One, .destinationColor = Device::BlendFactor::One, .sourceAlpha = Device::BlendFactor::One, .destinationAlpha = Device::BlendFactor::One};
    const std::array<Device::ColorTarget, 4> formats{{{.blend = adding}, {.writeMask = Device::ColorMask::Red}, {}, {}}};
    const Device::Pipeline pipeline = device().makePipeline({.program = program("targets"), .colors = formats, .label = "haylen-test-independent"});
    const std::array<Device::ColorAttachment, 4> colors{{{.image = targets[0], .clear = {0.0F, 0.5F, 0.0F, 0.0F}}, {.image = targets[1], .clear = math::Color::transparent()}, {.image = targets[2]}, {.image = targets[3]}}};

    device().beginPass({.colors = colors});
    device().applyPipeline(pipeline);
    device().applyBindings({});
    device().draw(0, 3, 1);
    device().endPass();
    device().commit();

    EXPECT_TRUE(isNear(read(targets[0], 1, 1).at(0, 0), {1.0F, 0.5F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(read(targets[1], 1, 1).at(0, 0), {0.0F, 0.0F, 0.0F, 0.0F}));
    device().destroy(pipeline);
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuBlendTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuBlendTest);

} // namespace haylen::gpu
