#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "gpu/support/GpuFixture.hpp"

namespace haylen::gpu {

class GpuDrawTest : public test::GpuFixture {
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

    void SetUp() override {
        GpuFixture::SetUp();
        if (!IsSkipped() && !HasFatalFailure()) {
            target = makeTarget(kSize, kSize);
        }
    }

    // A pipeline of the color program, with an offset per instance when the program is the instanced one.
    [[nodiscard]] Device::Pipeline makeColorPipeline(std::string_view name, Device::Primitive primitive = Device::Primitive::Triangles, Device::IndexType indexType = Device::IndexType::None) {
        std::vector<Device::VertexBufferLayout> buffers{{.stride = sizeof(Vertex)}};
        std::vector<Device::VertexAttribute> attributes{{.location = attribute(name, "position"), .format = Device::VertexFormat::Float3}, {.location = attribute(name, "color0"), .offset = 12, .format = Device::VertexFormat::Float4}};
        if (name == "instanced") {
            buffers.push_back({.stride = 8, .perInstance = true});
            attributes.push_back({.location = attribute(name, "instance_offset"), .buffer = 1, .format = Device::VertexFormat::Float2});
        }
        const std::array<Device::ColorTarget, 1> colors{};
        return device().makePipeline({.program = program(name), .buffers = buffers, .attributes = attributes, .primitive = primitive, .indexType = indexType, .colors = colors, .label = "haylen-test-color"});
    }

    [[nodiscard]] Device::Buffer makeVertices(std::span<const Vertex> vertices, Device::Usage usage = Device::Usage::Immutable) {
        if (usage == Device::Usage::Dynamic) {
            return device().makeBuffer({.size = vertices.size_bytes(), .usage = usage, .label = "haylen-test-vertices"});
        }
        return device().makeBuffer({.data = std::as_bytes(vertices), .label = "haylen-test-vertices"});
    }

    void beginPass() {
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = math::Color::black()}}};
        device().beginPass({.colors = colors, .label = "haylen-test-draw"});
    }

    void applyColorParams(const ColorParams& params) {
        device().applyUniforms(0, std::as_bytes(std::span(&params, 1)));
    }

    [[nodiscard]] Pixels finish() {
        device().endPass();
        device().commit();
        return read(target, kSize, kSize);
    }

    // A square over the whole target as two triangles, and as four corners for strips and indices. The small triangles of the tests sit in the top-left corner, where the pixels at 0, 1 lie inside them.
    static constexpr std::array<Vertex, 6> kSquare{{
        {-1.0F, -1.0F, 0.0F, {1.0F, 0.0F, 0.0F, 1.0F}},
        {1.0F, -1.0F, 0.0F, {1.0F, 0.0F, 0.0F, 1.0F}},
        {-1.0F, 1.0F, 0.0F, {1.0F, 0.0F, 0.0F, 1.0F}},
        {-1.0F, 1.0F, 0.0F, {1.0F, 0.0F, 0.0F, 1.0F}},
        {1.0F, -1.0F, 0.0F, {1.0F, 0.0F, 0.0F, 1.0F}},
        {1.0F, 1.0F, 0.0F, {1.0F, 0.0F, 0.0F, 1.0F}},
    }};
    static constexpr std::array<Vertex, 4> kCorners{{
        {-1.0F, -1.0F, 0.0F, {0.0F, 1.0F, 0.0F, 1.0F}},
        {1.0F, -1.0F, 0.0F, {0.0F, 1.0F, 0.0F, 1.0F}},
        {-1.0F, 1.0F, 0.0F, {0.0F, 1.0F, 0.0F, 1.0F}},
        {1.0F, 1.0F, 0.0F, {0.0F, 1.0F, 0.0F, 1.0F}},
    }};

    Device::Image target;
};

// Triangles and a strip of the vertex buffer cover the target, with the color of their vertices.
TEST_P(GpuDrawTest, DrawsTrianglesAndStripsFromVertexBuffers) {
    const Device::Pipeline triangles = makeColorPipeline("color");
    const Device::Pipeline strip = makeColorPipeline("color", Device::Primitive::TriangleStrip);
    const Device::Buffer square = makeVertices(kSquare);
    const Device::Buffer corners = makeVertices(kCorners);

    beginPass();
    device().applyPipeline(triangles);
    Device::Bindings bindings;
    bindings.vertexBuffers[0] = square;
    device().applyBindings(bindings);
    applyColorParams({});
    device().draw(0, 6, 1);
    device().applyViewport({0.0F, 0.0F, static_cast<float>(kSize), static_cast<float>(kSize) / 2.0F});
    device().applyPipeline(strip);
    bindings.vertexBuffers[0] = corners;
    device().applyBindings(bindings);
    applyColorParams({});
    device().draw(0, 4, 1);
    const Pixels pixels = finish();

    EXPECT_TRUE(isNear(pixels.at(4, 1), {0.0F, 1.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(4, 6), {1.0F, 0.0F, 0.0F, 1.0F}));
    for (const Device::Buffer buffer : {square, corners}) {
        device().destroy(buffer);
    }
    device().destroy(triangles);
    device().destroy(strip);
}

// Indices of 16 and 32 bits pick the corners of the square, from an offset into the index buffer.
TEST_P(GpuDrawTest, DrawsIndexedTrianglesOfBothIndexSizes) {
    const Device::Buffer corners = makeVertices(kCorners);
    const std::array<std::uint16_t, 8> shortIndices{9, 9, 0, 1, 2, 2, 1, 3};
    const std::array<std::uint32_t, 8> longIndices{9, 9, 0, 1, 2, 2, 1, 3};
    const Device::Buffer shortBuffer = device().makeBuffer({.data = std::as_bytes(std::span(shortIndices)), .index = true, .label = "haylen-test-short-indices"});
    const Device::Buffer longBuffer = device().makeBuffer({.data = std::as_bytes(std::span(longIndices)), .index = true, .label = "haylen-test-long-indices"});

    for (const Device::IndexType type : {Device::IndexType::UInt16, Device::IndexType::UInt32}) {
        const Device::Pipeline pipeline = makeColorPipeline("color", Device::Primitive::Triangles, type);
        const bool shortType = type == Device::IndexType::UInt16;
        beginPass();
        device().applyPipeline(pipeline);
        Device::Bindings bindings;
        bindings.vertexBuffers[0] = corners;
        bindings.indexBuffer = shortType ? shortBuffer : longBuffer;
        bindings.indexOffset = shortType ? 2 : 4;
        device().applyBindings(bindings);
        applyColorParams({});
        device().draw(1, 6, 1);
        const Pixels pixels = finish();
        EXPECT_TRUE(isNear(pixels.at(0, 0), {0.0F, 1.0F, 0.0F, 1.0F})) << static_cast<int>(type);
        EXPECT_TRUE(isNear(pixels.at(7, 7), {0.0F, 1.0F, 0.0F, 1.0F})) << static_cast<int>(type);
        device().destroy(pipeline);
    }
    for (const Device::Buffer buffer : {corners, shortBuffer, longBuffer}) {
        device().destroy(buffer);
    }
}

// Every instance of a draw takes its own offset from the buffer that steps per instance.
TEST_P(GpuDrawTest, DrawsInstancesWithTheirOwnAttributes) {
    const Device::Pipeline pipeline = makeColorPipeline("instanced");
    const std::array<Vertex, 3> corner{{
        {-1.0F, 0.5F, 0.0F, {0.0F, 0.0F, 1.0F, 1.0F}},
        {-0.5F, 0.5F, 0.0F, {0.0F, 0.0F, 1.0F, 1.0F}},
        {-1.0F, 1.0F, 0.0F, {0.0F, 0.0F, 1.0F, 1.0F}},
    }};
    const std::array<float, 4> offsets{0.0F, 0.0F, 1.5F, -1.5F};
    const Device::Buffer vertices = makeVertices(corner);
    const Device::Buffer instances = device().makeBuffer({.data = std::as_bytes(std::span(offsets)), .label = "haylen-test-instances"});

    beginPass();
    device().applyPipeline(pipeline);
    Device::Bindings bindings;
    bindings.vertexBuffers[0] = vertices;
    bindings.vertexBuffers[1] = instances;
    device().applyBindings(bindings);
    device().draw(0, 3, 2);
    const Pixels pixels = finish();

    EXPECT_TRUE(isNear(pixels.at(0, 1), {0.0F, 0.0F, 1.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(6, 7), {0.0F, 0.0F, 1.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(6, 0), math::Color::black()));
    device().destroy(vertices);
    device().destroy(instances);
    device().destroy(pipeline);
}

// Each draw reads the uniforms applied for it, so two draws of one buffer land apart in their own tints.
TEST_P(GpuDrawTest, AppliesUniformsPerDraw) {
    const Device::Pipeline pipeline = makeColorPipeline("color");
    const std::array<Vertex, 3> corner{{
        {-1.0F, 0.5F, 0.0F, {1.0F, 1.0F, 1.0F, 1.0F}},
        {-0.5F, 0.5F, 0.0F, {1.0F, 1.0F, 1.0F, 1.0F}},
        {-1.0F, 1.0F, 0.0F, {1.0F, 1.0F, 1.0F, 1.0F}},
    }};
    const Device::Buffer vertices = makeVertices(corner);

    beginPass();
    device().applyPipeline(pipeline);
    Device::Bindings bindings;
    bindings.vertexBuffers[0] = vertices;
    device().applyBindings(bindings);
    applyColorParams({.tint = {1.0F, 0.0F, 0.0F, 1.0F}});
    device().draw(0, 3, 1);
    applyColorParams({.offset = {1.5F, -1.5F, 0.0F, 0.0F}, .tint = {0.0F, 1.0F, 0.0F, 1.0F}});
    device().draw(0, 3, 1);
    const Pixels pixels = finish();

    EXPECT_TRUE(isNear(pixels.at(0, 1), {1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_TRUE(isNear(pixels.at(6, 7), {0.0F, 1.0F, 0.0F, 1.0F}));
    device().destroy(vertices);
    device().destroy(pipeline);
}

// A line across the middle of the target lights the pixels it passes.
TEST_P(GpuDrawTest, DrawsLines) {
    const Device::Pipeline pipeline = makeColorPipeline("color", Device::Primitive::Lines);
    const float middle = 1.0F / static_cast<float>(kSize);
    const std::array<Vertex, 2> line{{{-1.0F, middle, 0.0F, math::Color::white()}, {1.0F, middle, 0.0F, math::Color::white()}}};
    const Device::Buffer vertices = makeVertices(line);

    beginPass();
    device().applyPipeline(pipeline);
    Device::Bindings bindings;
    bindings.vertexBuffers[0] = vertices;
    device().applyBindings(bindings);
    applyColorParams({});
    device().draw(0, 2, 1);
    const Pixels pixels = finish();

    int lit = 0;
    for (int x = 0; x < kSize; ++x) {
        lit += pixels.at(x, 3).r > 0.5F ? 1 : 0;
    }
    EXPECT_GE(lit, kSize - 1);
    EXPECT_TRUE(isNear(pixels.at(4, 0), math::Color::black()));
    device().destroy(vertices);
    device().destroy(pipeline);
}

// A dynamic buffer takes new contents every frame, which the draws of that frame read.
TEST_P(GpuDrawTest, ReadsTheContentsOfDynamicBuffersOfEachFrame) {
    const Device::Pipeline pipeline = makeColorPipeline("color");
    const Device::Buffer vertices = makeVertices(kSquare, Device::Usage::Dynamic);
    for (const math::Color color : {math::Color{1.0F, 0.0F, 0.0F, 1.0F}, math::Color{0.0F, 0.0F, 1.0F, 1.0F}, math::Color{0.0F, 1.0F, 1.0F, 1.0F}}) {
        std::array<Vertex, 6> square = kSquare;
        for (Vertex& vertex : square) {
            vertex.color = color;
        }
        device().writeBuffer(vertices, 0, std::as_bytes(std::span(square)));
        beginPass();
        device().applyPipeline(pipeline);
        Device::Bindings bindings;
        bindings.vertexBuffers[0] = vertices;
        device().applyBindings(bindings);
        applyColorParams({});
        device().draw(0, 6, 1);
        EXPECT_TRUE(isNear(finish().at(4, 4), color)) << color.toHex();
    }
    device().destroy(vertices);
    device().destroy(pipeline);
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuDrawTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuDrawTest);

} // namespace haylen::gpu
