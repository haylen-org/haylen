#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <span>

#include "gpu/support/GpuFixture.hpp"

namespace haylen::gpu {

class GpuDepthStencilTest : public test::GpuFixture {
  protected:
    static constexpr int kSize = 8;

    // The depth format of the tests, and the one with stencil, which is 24-bit depth where the device renders it and 32-bit float depth in its place elsewhere.
    [[nodiscard]] Device::PixelFormat getDepthFormat() const {
        return capabilities().renderFormats[static_cast<std::size_t>(Device::PixelFormat::Depth32Float)] ? Device::PixelFormat::Depth32Float : Device::PixelFormat::Depth16;
    }

    [[nodiscard]] Device::PixelFormat getStencilFormat() const {
        return capabilities().renderFormats[static_cast<std::size_t>(Device::PixelFormat::Depth24Stencil8)] ? Device::PixelFormat::Depth24Stencil8 : Device::PixelFormat::Depth32FloatStencil8;
    }

    [[nodiscard]] Device::Image makeDepth(Device::PixelFormat format) {
        return makeImage({.width = kSize, .height = kSize, .format = format, .kind = Device::ImageKind::DepthStencil, .label = "haylen-test-depth"});
    }
};

// A plane that lies behind what the depth attachment holds draws nothing, and one in front draws over it.
TEST_P(GpuDepthStencilTest, TestsAndWritesDepth) {
    const Device::PixelFormat format = getDepthFormat();
    const Device::Image color = makeTarget(kSize, kSize);
    const Device::Image depth = makeDepth(format);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline pipeline = makePlanePipeline(formats, {.format = format, .depthCompare = Device::CompareFunction::Less, .depthWrite = true});
    const std::array<Device::ColorAttachment, 1> colors{{{.image = color, .clear = math::Color::black()}}};

    // The depths keep their order whether the API maps clip depth from 0 or from -1.
    device().beginPass({.colors = colors, .depthStencil = Device::DepthStencilAttachment{.image = depth, .clearDepth = 1.0F}});
    device().applyPipeline(pipeline);
    drawPlane({1.0F, 0.0F, 0.0F, 1.0F}, 0.5F);
    drawPlane({0.0F, 1.0F, 0.0F, 1.0F}, 0.75F);
    device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize) / 2.0F, static_cast<float>(kSize)});
    drawPlane({0.0F, 0.0F, 1.0F, 1.0F}, 0.25F);
    device().endPass();
    device().commit();

    const Pixels pixels = read(color, kSize, kSize);
    EXPECT_TRUE(isNear(pixels.at(1, 4), {0.0F, 0.0F, 1.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(6, 4), {1.0F, 0.0F, 0.0F, 1.0F}));
}

// A depth attachment cleared to the near end turns every plane away, and one that keeps its depth tests the next pass against it.
TEST_P(GpuDepthStencilTest, ClearsAndKeepsDepthAsItsLoadsSay) {
    const Device::PixelFormat format = getDepthFormat();
    const Device::Image color = makeTarget(kSize, kSize);
    const Device::Image depth = makeDepth(format);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline writing = makePlanePipeline(formats, {.format = format, .depthCompare = Device::CompareFunction::Always, .depthWrite = true});
    const Device::Pipeline testing = makePlanePipeline(formats, {.format = format, .depthCompare = Device::CompareFunction::Less});
    const std::array<Device::ColorAttachment, 1> cleared{{{.image = color, .clear = math::Color::black()}}};
    const std::array<Device::ColorAttachment, 1> kept{{{.image = color, .load = Device::Load::Keep}}};

    device().beginPass({.colors = cleared, .depthStencil = Device::DepthStencilAttachment{.image = depth, .depthStore = Device::Store::Store, .clearDepth = 1.0F}});
    device().applyPipeline(writing);
    device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize) / 2.0F, static_cast<float>(kSize)});
    drawPlane({0.0F, 0.0F, 0.0F, 1.0F}, 0.25F);
    device().endPass();

    device().beginPass({.colors = kept, .depthStencil = Device::DepthStencilAttachment{.image = depth, .depthLoad = Device::Load::Keep}});
    device().applyPipeline(testing);
    drawPlane({1.0F, 1.0F, 1.0F, 1.0F}, 0.5F);
    device().endPass();
    device().commit();

    Pixels pixels = read(color, kSize, kSize);
    EXPECT_TRUE(isNear(pixels.at(1, 4), math::Color::black()));
    EXPECT_TRUE(isNear(pixels.at(6, 4), math::Color::white()));

    device().beginPass({.colors = cleared, .depthStencil = Device::DepthStencilAttachment{.image = depth, .clearDepth = 0.0F}});
    device().applyPipeline(testing);
    drawPlane({1.0F, 1.0F, 1.0F, 1.0F}, 0.5F);
    device().endPass();
    device().commit();
    pixels = read(color, kSize, kSize);
    EXPECT_TRUE(isNear(pixels.at(6, 4), math::Color::black()));
}

// A first plane marks the stencil of the left half, and a second one draws only where the stencil equals the reference.
TEST_P(GpuDepthStencilTest, TestsTheStencilAgainstItsReference) {
    const Device::PixelFormat format = getStencilFormat();
    const Device::Image color = makeTarget(kSize, kSize);
    const Device::Image depth = makeDepth(format);
    const std::array<Device::ColorTarget, 1> formats{{{.writeMask = Device::ColorMask::None}}};
    const std::array<Device::ColorTarget, 1> drawing{};
    const Device::StencilFace marking{.compare = Device::CompareFunction::Always, .pass = Device::StencilOperation::Replace};
    const Device::StencilFace testing{.compare = Device::CompareFunction::Equal};
    const Device::Pipeline mark = makePlanePipeline(formats, {.format = format, .stencilEnabled = true, .stencilFront = marking, .stencilBack = marking});
    const Device::Pipeline test = makePlanePipeline(drawing, {.format = format, .stencilEnabled = true, .stencilFront = testing, .stencilBack = testing});
    const std::array<Device::ColorAttachment, 1> colors{{{.image = color, .clear = math::Color::black()}}};

    device().beginPass({.colors = colors, .depthStencil = Device::DepthStencilAttachment{.image = depth, .clearStencil = 0}});
    device().applyPipeline(mark);
    device().applyStencilReference(7);
    device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize) / 2.0F, static_cast<float>(kSize)});
    drawPlane(math::Color::white());
    device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize), static_cast<float>(kSize)});
    device().applyPipeline(test);
    device().applyStencilReference(7);
    drawPlane({0.0F, 1.0F, 0.0F, 1.0F});
    device().endPass();
    device().commit();

    const Pixels pixels = read(color, kSize, kSize);
    EXPECT_TRUE(isNear(pixels.at(1, 4), {0.0F, 1.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(6, 4), math::Color::black()));
}

// A sampler that compares turns the depths of a depth image into lit and unlit against the reference of the draw.
TEST_P(GpuDepthStencilTest, SamplesDepthThroughAComparison) {
    const Device::PixelFormat format = getDepthFormat();
    if (!capabilities().textureFormats[static_cast<std::size_t>(format)]) {
        GTEST_SKIP() << "The device samples no image of the depth format of the tests.";
    }
    const Device::Image depth = makeDepth(format);
    device().beginPass({.depthStencil = Device::DepthStencilAttachment{.image = depth, .depthStore = Device::Store::Store, .clearDepth = 0.5F}});
    device().endPass();

    const Device::Image color = makeTarget(1, 1);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline pipeline = device().makePipeline({.program = program("shadow"), .colors = formats, .label = "haylen-test-shadow"});
    const Device::Sampler sampler = device().makeSampler({.minification = Device::Filter::Nearest, .magnification = Device::Filter::Nearest, .compare = Device::CompareFunction::LessEqual});
    for (const auto& [reference, lit] : std::array<std::pair<float, bool>, 2>{{{0.25F, true}, {0.75F, false}}}) {
        const std::array<Device::ColorAttachment, 1> colors{{{.image = color}}};
        const std::array<float, 4> lookup{0.0F, 0.0F, reference, 1.0F};
        device().beginPass({.colors = colors});
        device().applyPipeline(pipeline);
        Device::Bindings bindings;
        bindings.textures[0] = depth;
        bindings.samplers[0] = sampler;
        device().applyBindings(bindings);
        device().applyUniforms(0, std::as_bytes(std::span(lookup)));
        device().draw(0, 3, 1);
        device().endPass();
        device().commit();
        EXPECT_TRUE(isNear(read(color, 1, 1).at(0, 0), lit ? math::Color::white() : math::Color::black())) << reference;
    }
    device().destroy(sampler);
    device().destroy(pipeline);
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuDepthStencilTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuDepthStencilTest);

} // namespace haylen::gpu
