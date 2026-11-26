#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "gpu/support/GpuFixture.hpp"
#include "support/ContractRule.hpp"

namespace haylen::gpu {

class GpuBufferTest : public test::GpuFixture {
  protected:
    static constexpr int kSize = 8;

    // A vertex of the color program: its position in clip space with its depth, and its color.
    struct Vertex {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        math::Color color;
    };

    // The uniforms of the color program: the offset of the draw and its tint.
    struct ColorParams {
        std::array<float, 4> offset{};
        std::array<float, 4> tint{1.0F, 1.0F, 1.0F, 1.0F};
    };

    void TearDown() override {
        if (!IsSkipped()) {
            for (const Device::Buffer buffer : buffers) {
                device().destroy(buffer);
            }

            if (pipeline.isValid()) {
                device().destroy(pipeline);
            }
        }

        GpuFixture::TearDown();
    }

    [[nodiscard]] Device::Buffer makeBuffer(const Device::BufferDesc& desc) {
        buffers.push_back(device().makeBuffer(desc));
        return buffers.back();
    }

    [[nodiscard]] Device::Pipeline getColorPipeline() {
        if (!pipeline.isValid()) {
            const std::array<Device::VertexBufferLayout, 1> layouts{{{.stride = sizeof(Vertex)}}};
            const std::array<Device::VertexAttribute, 2> attributes{{{.location = attribute("color", "position"), .format = Device::VertexFormat::Float3}, {.location = attribute("color", "color0"), .offset = 12, .format = Device::VertexFormat::Float4}}};
            const std::array<Device::ColorTarget, 1> colors{};
            pipeline = device().makePipeline({.program = program("color"), .buffers = layouts, .attributes = attributes, .colors = colors, .label = "haylen-test-buffers"});
        }

        return pipeline;
    }

    // A square over the whole target in the color.
    [[nodiscard]] static std::array<Vertex, 6> makeSquare(math::Color color) {
        return {{{-1.0F, -1.0F, 0.0F, color}, {1.0F, -1.0F, 0.0F, color}, {-1.0F, 1.0F, 0.0F, color}, {-1.0F, 1.0F, 0.0F, color}, {1.0F, -1.0F, 0.0F, color}, {1.0F, 1.0F, 0.0F, color}}};
    }

    void drawSquare(Device::Image target, Device::Buffer vertices) {
        drawSquare(target, vertices, Device::Bindings{}, ColorParams{});
    }

    void drawSquare(Device::Image target, Device::Buffer vertices, const Device::Bindings& uniforms) {
        drawSquare(target, vertices, uniforms, ColorParams{});
    }

    // Draws the six vertices of the buffer into the target, with the uniforms of the bindings or, when they name no uniform buffer, the ones given.
    void drawSquare(Device::Image target, Device::Buffer vertices, const Device::Bindings& uniforms, const ColorParams& params) {
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = math::Color::black()}}};
        device().beginPass({.colors = colors, .label = "haylen-test-buffer-pass"});
        device().applyPipeline(getColorPipeline());
        Device::Bindings bindings = uniforms;
        bindings.vertexBuffers[0] = vertices;
        device().applyBindings(bindings);
        if (!bindings.uniformBuffers[0].buffer.isValid()) {
            device().applyUniforms(0, std::as_bytes(std::span(&params, 1)));
        }

        device().draw(0, 6, 1);
        device().endPass();
    }

    [[nodiscard]] std::vector<std::uint8_t> readBytes(Device::Buffer buffer, std::size_t offset, std::size_t size) {
        const std::unique_ptr<Device::Readback> copy = device().readBuffer(buffer, offset, size);
        return finishCopy(*copy);
    }

    [[nodiscard]] static std::vector<std::uint8_t> toBytes(std::span<const std::byte> values) {
        std::vector<std::uint8_t> bytes(values.size());
        std::memcpy(bytes.data(), values.data(), bytes.size());
        return bytes;
    }

    std::vector<Device::Buffer> buffers;
    Device::Pipeline pipeline;
};

// A dynamic buffer keeps its contents across frames, and each write reaches only the passes after it, so two passes of one frame draw the contents that each one followed.
TEST_P(GpuBufferTest, OrdersTheWritesOfDynamicBuffersWithThePassesAroundThem) {
    const Device::Image first = makeTarget(kSize, kSize);
    const Device::Image second = makeTarget(kSize, kSize);
    const Device::Image later = makeTarget(kSize, kSize);
    const std::array<Vertex, 6> red = makeSquare({1.0F, 0.0F, 0.0F, 1.0F});
    const std::array<Vertex, 6> green = makeSquare({0.0F, 1.0F, 0.0F, 1.0F});
    const Device::Buffer vertices = makeBuffer({.data = std::as_bytes(std::span(red)), .usage = {.vertex = true}, .update = Device::BufferUpdate::Dynamic, .label = "haylen-test-dynamic"});

    drawSquare(first, vertices);
    device().writeBuffer(vertices, 0, std::as_bytes(std::span(green)));
    drawSquare(second, vertices);
    device().commit();
    drawSquare(later, vertices);
    device().commit();

    EXPECT_TRUE(isNear(read(first, kSize, kSize).at(4, 4), {1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(read(second, kSize, kSize).at(4, 4), {0.0F, 1.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(read(later, kSize, kSize).at(4, 4), {0.0F, 1.0F, 0.0F, 1.0F}));
}

// A dynamic buffer takes writes of parts of it, which keep the rest of its contents.
TEST_P(GpuBufferTest, WritesRangesOfDynamicBuffers) {
    const std::array<std::uint32_t, 8> start{1, 2, 3, 4, 5, 6, 7, 8};
    const std::array<std::uint32_t, 3> part{20, 30, 40};
    const Device::Buffer buffer = makeBuffer({.data = std::as_bytes(std::span(start)), .usage = {.copySource = true}, .update = Device::BufferUpdate::Dynamic, .label = "haylen-test-ranges"});

    device().writeBuffer(buffer, 8, std::as_bytes(std::span(part)));
    device().commit();

    const std::array<std::uint32_t, 8> expected{1, 2, 20, 30, 40, 6, 7, 8};
    EXPECT_EQ(readBytes(buffer, 0, sizeof(expected)), toBytes(std::as_bytes(std::span(expected))));
    EXPECT_EQ(readBytes(buffer, 12, 8), toBytes(std::as_bytes(std::span(expected).subspan(3, 2))));
}

// A copy moves a range of one buffer into another between passes, and a readback hands the bytes over as they are.
TEST_P(GpuBufferTest, CopiesRangesBetweenBuffersAndReadsThemBack) {
    std::array<std::uint32_t, 64> source{};
    for (std::size_t index = 0; index < source.size(); ++index) {
        source[index] = static_cast<std::uint32_t>(index * 3 + 1);
    }

    const Device::Buffer from = makeBuffer({.data = std::as_bytes(std::span(source)), .usage = {.copySource = true}, .label = "haylen-test-copy-source"});
    const Device::Buffer to = makeBuffer({.size = sizeof(source), .usage = {.copySource = true, .copyDestination = true}, .update = Device::BufferUpdate::Dynamic, .label = "haylen-test-copy-destination"});
    device().copyBuffer(from, 16, to, 64, 128);
    device().copyBuffer(from, 0, to, 0, 16);
    device().commit();

    EXPECT_EQ(readBytes(to, 64, 128), toBytes(std::as_bytes(std::span(source).subspan(4, 32))));
    EXPECT_EQ(readBytes(to, 0, 16), toBytes(std::as_bytes(std::span(source).subspan(0, 4))));
    EXPECT_EQ(readBytes(from, 0, sizeof(source)), toBytes(std::as_bytes(std::span(source))));
}

// A copy into a vertex buffer between two passes reaches the draws of the second pass alone.
TEST_P(GpuBufferTest, DrawsTheVerticesThatACopyBroughtBetweenPasses) {
    const Device::Image first = makeTarget(kSize, kSize);
    const Device::Image second = makeTarget(kSize, kSize);
    const std::array<Vertex, 6> red = makeSquare({1.0F, 0.0F, 0.0F, 1.0F});
    const std::array<Vertex, 6> blue = makeSquare({0.0F, 0.0F, 1.0F, 1.0F});
    const Device::Buffer staged = makeBuffer({.data = std::as_bytes(std::span(blue)), .usage = {.copySource = true}, .label = "haylen-test-staged"});
    const Device::Buffer vertices = makeBuffer({.data = std::as_bytes(std::span(red)), .usage = {.vertex = true, .copyDestination = true}, .update = Device::BufferUpdate::Dynamic, .label = "haylen-test-copied-vertices"});

    drawSquare(first, vertices);
    device().copyBuffer(staged, 0, vertices, 0, sizeof(blue));
    drawSquare(second, vertices);
    device().commit();

    EXPECT_TRUE(isNear(read(first, kSize, kSize).at(4, 4), {1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(read(second, kSize, kSize).at(4, 4), {0.0F, 0.0F, 1.0F, 1.0F}));
}

// A uniform block reads its bytes from a range of a uniform buffer, whose ranges at aligned offsets feed draws apart, and a dynamic uniform buffer that changes between passes feeds the later pass its new bytes.
TEST_P(GpuBufferTest, FeedsUniformBlocksFromRangesOfUniformBuffers) {
    const Device::Image first = makeTarget(kSize, kSize);
    const Device::Image second = makeTarget(kSize, kSize);
    const Device::Image third = makeTarget(kSize, kSize);
    std::vector<std::byte> blocks(Device::kUniformBufferAlignment * 2);
    const ColorParams magenta{.tint = {1.0F, 0.0F, 1.0F, 1.0F}};
    const ColorParams yellow{.tint = {1.0F, 1.0F, 0.0F, 1.0F}};
    std::memcpy(blocks.data(), &magenta, sizeof(magenta));
    std::memcpy(blocks.data() + Device::kUniformBufferAlignment, &yellow, sizeof(yellow));
    const std::array<Vertex, 6> white = makeSquare(math::Color::white());
    const Device::Buffer vertices = makeBuffer({.data = std::as_bytes(std::span(white)), .usage = {.vertex = true}, .label = "haylen-test-white"});
    const Device::Buffer uniforms = makeBuffer({.data = blocks, .usage = {.uniform = true}, .update = Device::BufferUpdate::Dynamic, .label = "haylen-test-uniforms"});

    Device::Bindings atStart;
    atStart.uniformBuffers[0] = {.buffer = uniforms, .offset = 0};
    Device::Bindings atSecond;
    atSecond.uniformBuffers[0] = {.buffer = uniforms, .offset = Device::kUniformBufferAlignment};
    drawSquare(first, vertices, atStart);
    drawSquare(second, vertices, atSecond);
    const ColorParams cyan{.tint = {0.0F, 1.0F, 1.0F, 1.0F}};
    device().writeBuffer(uniforms, 0, std::as_bytes(std::span(&cyan, 1)));
    drawSquare(third, vertices, atStart);
    device().commit();

    EXPECT_TRUE(isNear(read(first, kSize, kSize).at(4, 4), {1.0F, 0.0F, 1.0F, 1.0F}));
    EXPECT_TRUE(isNear(read(second, kSize, kSize).at(4, 4), {1.0F, 1.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(read(third, kSize, kSize).at(4, 4), {0.0F, 1.0F, 1.0F, 1.0F}));
}

// A stream uniform buffer holds what the frame wrote into it, which the draws of that frame read.
TEST_P(GpuBufferTest, FeedsUniformBlocksFromStreamBuffersOfEachFrame) {
    const std::array<Vertex, 6> white = makeSquare(math::Color::white());
    const Device::Buffer vertices = makeBuffer({.data = std::as_bytes(std::span(white)), .usage = {.vertex = true}, .label = "haylen-test-white"});
    const Device::Buffer uniforms = makeBuffer({.size = sizeof(ColorParams), .usage = {.uniform = true}, .update = Device::BufferUpdate::Stream, .label = "haylen-test-stream-uniforms"});
    Device::Bindings bindings;
    bindings.uniformBuffers[0] = {.buffer = uniforms};
    for (const math::Color color : {math::Color{1.0F, 0.0F, 0.0F, 1.0F}, math::Color{0.0F, 0.0F, 1.0F, 1.0F}}) {
        const Device::Image target = makeTarget(kSize, kSize);
        const ColorParams params{.tint = {color.r, color.g, color.b, color.a}};
        device().writeBuffer(uniforms, 0, std::as_bytes(std::span(&params, 1)));
        drawSquare(target, vertices, bindings);
        device().commit();
        EXPECT_TRUE(isNear(read(target, kSize, kSize).at(4, 4), color)) << color.toHex();
    }
}

// Every device refuses what breaks the usages and the update of a buffer, with the rule that it breaks.
TEST_P(GpuBufferTest, RefusesWhatTheUsagesAndTheUpdateOfABufferForbid) {
    const std::array<std::uint32_t, 4> words{};
    const auto bytes = std::as_bytes(std::span(words));
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeBuffer({.size = 64, .label = "haylen-test-unused"}); }), ContractError::Rule::Buffer);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeBuffer({.size = 64, .usage = {.copySource = true}, .update = Device::BufferUpdate::Stream, .label = "haylen-test-copied-stream"}); }), ContractError::Rule::Buffer);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().makeBuffer({.size = 64, .usage = {.vertex = true}, .label = "haylen-test-empty-immutable"}); }), ContractError::Rule::Buffer);

    const Device::Buffer fixed = makeBuffer({.data = bytes, .usage = {.vertex = true}, .label = "haylen-test-fixed"});
    const Device::Buffer dynamic = makeBuffer({.size = 64, .usage = {.copyDestination = true}, .update = Device::BufferUpdate::Dynamic, .label = "haylen-test-dynamic-rules"});
    EXPECT_EQ(test::ContractRule::of([&] { device().writeBuffer(fixed, 0, bytes); }), ContractError::Rule::BufferWrite);
    EXPECT_EQ(test::ContractRule::of([&] { device().writeBuffer(dynamic, 0, bytes.first(6)); }), ContractError::Rule::BufferWrite);
    EXPECT_EQ(test::ContractRule::of([&] { device().writeBuffer(dynamic, 56, bytes); }), ContractError::Rule::BufferWrite);
    EXPECT_EQ(test::ContractRule::of([&] { device().copyBuffer(fixed, 0, dynamic, 0, 16); }), ContractError::Rule::BufferWrite);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().readBuffer(dynamic, 0, 16); }), ContractError::Rule::Readback);
    device().commit();
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuBufferTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuBufferTest);

} // namespace haylen::gpu
