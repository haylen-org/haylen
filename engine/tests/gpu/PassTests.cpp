#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>

#include "gpu/support/GpuFixture.hpp"

namespace haylen::gpu {

class GpuPassTest : public test::GpuFixture {
  protected:
    static constexpr int kSize = 8;

    // Opens a pass into the target that keeps what it holds, with the viewport over the whole target.
    void beginKeptPass(Device::Image target) {
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .load = Device::Load::Keep}}};
        device().beginPass({.colors = colors, .label = "haylen-test-kept"});
        device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize), static_cast<float>(kSize)});
    }
};

// A cleared render target reads back in the color of the clear, every pixel of it.
TEST_P(GpuPassTest, ClearsRenderTargetsToTheirColor) {
    const math::Color color{0.2F, 0.4F, 0.6F, 0.8F};
    const Pixels pixels = clearAndRead(color, 4, 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) {
            EXPECT_TRUE(isNear(pixels.at(x, y), color)) << x << ", " << y;
        }
    }
}

// The viewport and the scissor count from the top-left corner of the target, and the rows read back with the top row first on every API.
TEST_P(GpuPassTest, PlacesViewportsAndScissorsFromTheTopLeftCorner) {
    const Device::Image target = makeTarget(kSize, kSize);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline plane = makePlanePipeline(formats);
    const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = math::Color::black()}}};

    device().beginPass({.colors = colors});
    device().applyPipeline(plane);
    device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize), static_cast<float>(kSize) / 2.0F});
    drawPlane({1.0F, 0.0F, 0.0F, 1.0F});
    device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize), static_cast<float>(kSize)});
    device().applyScissor({6.0F, 6.0F, 2.0F, 2.0F});
    drawPlane({0.0F, 1.0F, 0.0F, 1.0F});
    device().endPass();
    device().commit();

    const Pixels pixels = read(target, kSize, kSize);
    EXPECT_TRUE(isNear(pixels.at(0, 0), {1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(7, 3), {1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(0, 4), math::Color::black()));
    EXPECT_TRUE(isNear(pixels.at(5, 5), math::Color::black()));
    EXPECT_TRUE(isNear(pixels.at(6, 6), {0.0F, 1.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(7, 7), {0.0F, 1.0F, 0.0F, 1.0F}));
}

// A pass that keeps what the target holds draws over the pass before it, in the same frame and in the next one.
TEST_P(GpuPassTest, KeepsWhatTheTargetHeldWhenItLoadsIt) {
    const Device::Image target = makeTarget(kSize, kSize);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline plane = makePlanePipeline(formats);
    const std::array<Device::ColorAttachment, 1> cleared{{{.image = target, .clear = {0.0F, 0.0F, 1.0F, 1.0F}}}};
    device().beginPass({.colors = cleared});
    device().endPass();

    beginKeptPass(target);
    device().applyPipeline(plane);
    device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize) / 2.0F, static_cast<float>(kSize)});
    drawPlane({1.0F, 1.0F, 0.0F, 1.0F});
    device().endPass();
    device().commit();

    beginKeptPass(target);
    device().applyPipeline(plane);
    device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize), 2.0F});
    drawPlane({1.0F, 0.0F, 1.0F, 1.0F});
    device().endPass();
    device().commit();

    const Pixels pixels = read(target, kSize, kSize);
    EXPECT_TRUE(isNear(pixels.at(1, 1), {1.0F, 0.0F, 1.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(1, 5), {1.0F, 1.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(6, 5), {0.0F, 0.0F, 1.0F, 1.0F}));
}

// One draw writes a color of its own into each of four targets.
TEST_P(GpuPassTest, DrawsIntoFourTargetsAtOnce) {
    std::array<Device::Image, Device::kMaxColorTargets> targets{};
    std::array<Device::ColorAttachment, Device::kMaxColorTargets> colors{};
    for (std::size_t index = 0; index < targets.size(); ++index) {
        targets[index] = makeTarget(kSize, kSize);
        colors[index] = {.image = targets[index], .clear = math::Color::transparent()};
    }
    const std::array<Device::ColorTarget, Device::kMaxColorTargets> formats{};
    const Device::Pipeline pipeline = device().makePipeline({.program = program("targets"), .colors = formats, .label = "haylen-test-targets"});

    device().beginPass({.colors = colors});
    device().applyPipeline(pipeline);
    device().applyBindings({});
    device().draw(0, 3, 1);
    device().endPass();
    device().commit();

    const std::array<math::Color, 4> expected{{{1.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}, math::Color::white()}};
    for (std::size_t index = 0; index < targets.size(); ++index) {
        EXPECT_TRUE(isNear(read(targets[index], kSize, kSize).at(3, 3), expected[index])) << index;
    }
    device().destroy(pipeline);
}

// A multisampled target resolves into a target of one sample, where the edge of a triangle covers part of the samples of its pixels.
TEST_P(GpuPassTest, ResolvesMultisampledTargets) {
    if (!capabilities().multisampledRenderTargets || capabilities().maxSampleCount < 4) {
        GTEST_SKIP() << "The device resolves no render target of four samples.";
    }
    const Device::Image samples = makeTarget(kSize, kSize, Device::PixelFormat::Rgba8, 4);
    const Device::Image resolved = makeTarget(kSize, kSize);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline pipeline = device().makePipeline({.program = program("color"), .buffers = std::array<Device::VertexBufferLayout, 1>{{{.stride = 28}}}, .attributes = std::array<Device::VertexAttribute, 2>{{{.location = attribute("color", "position"), .format = Device::VertexFormat::Float3}, {.location = attribute("color", "color0"), .offset = 12, .format = Device::VertexFormat::Float4}}}, .colors = formats, .sampleCount = 4, .label = "haylen-test-multisampled"});

    // The triangle covers the half of the target below its diagonal.
    const std::array<float, 21> vertices{-1.0F, -1.0F, 0.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, -1.0F, 0.0F, 1.0F, 1.0F, 1.0F, 1.0F, -1.0F, 1.0F, 0.0F, 1.0F, 1.0F, 1.0F, 1.0F};
    const Device::Buffer buffer = device().makeBuffer({.data = std::as_bytes(std::span(vertices)), .label = "haylen-test-triangle"});
    const std::array<float, 8> uniforms{0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 1.0F};
    const std::array<Device::ColorAttachment, 1> colors{{{.image = samples, .store = Device::Store::Discard, .clear = math::Color::black(), .resolve = resolved}}};

    device().beginPass({.colors = colors});
    device().applyPipeline(pipeline);
    Device::Bindings bindings;
    bindings.vertexBuffers[0] = buffer;
    device().applyBindings(bindings);
    device().applyUniforms(0, std::as_bytes(std::span(uniforms)));
    device().draw(0, 3, 1);
    device().endPass();
    device().commit();

    const Pixels pixels = read(resolved, kSize, kSize);
    EXPECT_TRUE(isNear(pixels.at(1, 6), math::Color::white()));
    EXPECT_TRUE(isNear(pixels.at(6, 1), math::Color::black()));
    int partial = 0;
    for (int index = 0; index < kSize; ++index) {
        const float gray = pixels.at(index, index).r;
        partial += gray > 0.1F && gray < 0.9F ? 1 : 0;
    }
    EXPECT_GT(partial, 0) << "No pixel on the edge of the triangle mixes its samples.";
    device().destroy(buffer);
    device().destroy(pipeline);
}

// A pass draws into a mip level and a layer of a render target, which a draw samples back.
TEST_P(GpuPassTest, DrawsIntoMipLevelsAndLayers) {
    const Device::Image layered = makeImage({.type = Device::TextureType::Array, .width = kSize, .height = kSize, .layers = 2, .mipLevels = 2, .kind = Device::ImageKind::RenderTarget, .label = "haylen-test-layered"});
    const std::array<std::array<Device::ColorAttachment, 1>, 4> parts{{
        {{{.image = layered, .mipLevel = 0, .layer = 0, .clear = {1.0F, 0.0F, 0.0F, 1.0F}}}},
        {{{.image = layered, .mipLevel = 0, .layer = 1, .clear = {0.0F, 1.0F, 0.0F, 1.0F}}}},
        {{{.image = layered, .mipLevel = 1, .layer = 0, .clear = {0.0F, 0.0F, 1.0F, 1.0F}}}},
        {{{.image = layered, .mipLevel = 1, .layer = 1, .clear = {1.0F, 1.0F, 0.0F, 1.0F}}}},
    }};
    for (const std::array<Device::ColorAttachment, 1>& part : parts) {
        device().beginPass({.colors = part});
        device().endPass();
    }

    const Device::Image target = makeTarget(1, 1);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline pipeline = device().makePipeline({.program = program("array"), .colors = formats, .label = "haylen-test-array"});
    const Device::Sampler sampler = device().makeSampler({.minification = Device::Filter::Nearest, .magnification = Device::Filter::Nearest});
    const std::array<math::Color, 4> expected{{{1.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}, {1.0F, 1.0F, 0.0F, 1.0F}}};
    for (std::size_t part = 0; part < parts.size(); ++part) {
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target}}};
        const std::array<float, 4> lookup{static_cast<float>(part / 2), static_cast<float>(part % 2), 0.0F, 1.0F};
        device().beginPass({.colors = colors});
        device().applyPipeline(pipeline);
        Device::Bindings bindings;
        bindings.textures[0] = layered;
        bindings.samplers[0] = sampler;
        device().applyBindings(bindings);
        device().applyUniforms(0, std::as_bytes(std::span(lookup)));
        device().draw(0, 3, 1);
        device().endPass();
        device().commit();
        EXPECT_TRUE(isNear(read(target, 1, 1).at(0, 0), expected[part])) << part;
    }
    device().destroy(sampler);
    device().destroy(pipeline);
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuPassTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuPassTest);

} // namespace haylen::gpu
