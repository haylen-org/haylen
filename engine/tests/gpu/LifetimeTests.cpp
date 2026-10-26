#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "core/EmbeddedFiles.hpp"
#include "gpu/support/GpuFixture.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Shader.hpp"
#include "support/AllocationTracker.hpp"
#include "support/VarnRuntime.hpp"

namespace haylen::gpu {

class GpuLifetimeTest : public test::GpuFixture {
  protected:
    static constexpr int kSize = 8;

    // Draws a frame of the plane into the target with the uniforms of the color and a dynamic buffer of the frame, like the steady frames of an app.
    void drawFrame(Device::Image target, Device::Pipeline pipeline, Device::Buffer buffer, math::Color color) {
        const std::array<float, 4> written{color.r, color.g, color.b, color.a};
        device().writeBuffer(buffer, 0, std::as_bytes(std::span(written)));
        const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = math::Color::black()}}};
        device().pushDebugGroup("haylen-test-frame");
        device().beginPass({.colors = colors, .label = "haylen-test-frame"});
        device().pushDebugGroup("haylen-test-planes");
        device().applyPipeline(pipeline);
        for (int draw = 0; draw < 4; ++draw) {
            drawPlane(color);
        }
        device().popDebugGroup();
        device().endPass();
        device().popDebugGroup();
        device().commit();
    }
};

// Objects destroyed right after the frame that uses them was committed stay alive for the GPU until that frame ends, so the frames draw what they recorded.
TEST_P(GpuLifetimeTest, DestroysObjectsWhileTheirFramesAreInFlight) {
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Image target = makeTarget(kSize, kSize);
    const Device::Sampler sampler = device().makeSampler({});
    for (int frame = 0; frame < 32; ++frame) {
        const math::Color color{static_cast<float>(frame % 4) / 3.0F, 0.5F, 1.0F, 1.0F};
        const std::array<std::byte, 4> texel{static_cast<std::byte>(frame * 8), std::byte{128}, std::byte{255}, std::byte{255}};
        const std::array<std::span<const std::byte>, 1> levels{texel};
        const Device::Image texture = device().makeImage({.width = 1, .height = 1, .levels = levels, .label = "haylen-test-transient"});
        const Device::Image transient = device().makeImage({.width = kSize, .height = kSize, .kind = Device::ImageKind::RenderTarget, .label = "haylen-test-transient-target"});
        const Device::Pipeline plane = device().makePipeline({.program = program("plane"), .colors = formats, .label = "haylen-test-transient-plane"});
        const Device::Pipeline copy = device().makePipeline({.program = program("textured"), .colors = formats, .label = "haylen-test-transient-copy"});

        const std::array<Device::ColorAttachment, 1> first{{{.image = transient, .clear = math::Color::black()}}};
        device().beginPass({.colors = first});
        device().applyPipeline(plane);
        drawPlane(color);
        device().endPass();

        const std::array<Device::ColorAttachment, 1> second{{{.image = target}}};
        const std::array<float, 4> lookup{0.0F, 0.0F, 0.0F, 1.0F};
        device().beginPass({.colors = second});
        device().applyPipeline(copy);
        Device::Bindings bindings;
        bindings.textures[0] = frame % 2 == 0 ? transient : texture;
        bindings.samplers[0] = sampler;
        device().applyBindings(bindings);
        device().applyUniforms(0, std::as_bytes(std::span(lookup)));
        device().draw(0, 3, 1);
        device().endPass();
        device().commit();

        device().destroy(texture);
        device().destroy(transient);
        device().destroy(plane);
        device().destroy(copy);
    }

    // The last frame copied the texel of frame 31.
    EXPECT_TRUE(isNear(read(target, kSize, kSize).at(4, 4), math::Color::fromRgba8(248, 128, 255, 255)));
    device().destroy(sampler);
}

// Frames that draw what the frames before them drew allocate nothing and keep every pool of the device as it was.
TEST_P(GpuLifetimeTest, RunsManyFramesWithoutGrowing) {
    const Device::Image target = makeTarget(kSize, kSize);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline pipeline = makePlanePipeline(formats);
    const Device::Buffer buffer = device().makeBuffer({.size = 64, .usage = Device::Usage::Dynamic, .label = "haylen-test-steady"});
    for (int frame = 0; frame < 60; ++frame) {
        drawFrame(target, pipeline, buffer, math::Color::white());
    }
    const std::vector<Device::Pool> before = device().getPools();

    const test::AllocationTracker tracker;
    for (int frame = 0; frame < 240; ++frame) {
        drawFrame(target, pipeline, buffer, math::Color::white());
    }
    EXPECT_EQ(tracker.getTotal(), 0U);

    const std::vector<Device::Pool> after = device().getPools();
    ASSERT_EQ(after.size(), before.size());
    for (std::size_t index = 0; index < after.size(); ++index) {
        EXPECT_EQ(after[index].used, before[index].used) << after[index].name;
    }
    EXPECT_TRUE(isNear(read(target, kSize, kSize).at(4, 4), math::Color::white()));
    device().destroy(buffer);
}

// A device that measures the GPU reports the time of the frames it finished.
TEST_P(GpuLifetimeTest, MeasuresTheFramesOfTheGpuWhereItCan) {
    if (!capabilities().gpuTiming) {
        EXPECT_FALSE(device().getGpuFrameSeconds().has_value());
        return;
    }
    const Device::Image target = makeTarget(kSize, kSize);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline pipeline = makePlanePipeline(formats);
    const Device::Buffer buffer = device().makeBuffer({.size = 64, .usage = Device::Usage::Dynamic, .label = "haylen-test-timed"});
    const auto deadline = std::chrono::steady_clock::now() + kReadTimeout;
    while (!device().getGpuFrameSeconds() && std::chrono::steady_clock::now() < deadline) {
        drawFrame(target, pipeline, buffer, math::Color::white());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(device().getGpuFrameSeconds().has_value());
    EXPECT_GE(*device().getGpuFrameSeconds(), 0.0);
    device().destroy(buffer);
}

// Programs prepared ahead on the I/O pool are made from the same descriptions and draw like the others.
TEST_P(GpuLifetimeTest, MakesProgramsThatItPreparedAhead) {
    test::VarnRuntime runtime;
    core::JobSystem jobs(runtime.getRuntime(), [](const std::string& error) { ADD_FAILURE() << error; });
    const graphics::Shader prepared = graphics::Shader::parse(core::EmbeddedFiles::getConformanceShader());
    prepared.getResource()->prepare(device(), jobs);

    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline pipeline = device().makePipeline({.program = prepared.getResource()->getProgram(device(), "plane").program, .colors = formats, .label = "haylen-test-prepared"});
    const Device::Image target = makeTarget(kSize, kSize);
    const std::array<Device::ColorAttachment, 1> colors{{{.image = target}}};
    device().beginPass({.colors = colors});
    device().applyPipeline(pipeline);
    drawPlane({0.0F, 1.0F, 0.0F, 1.0F});
    device().endPass();
    device().commit();
    EXPECT_TRUE(isNear(read(target, kSize, kSize).at(4, 4), {0.0F, 1.0F, 0.0F, 1.0F}));

    device().destroy(pipeline);
    device().destroy(prepared.getResource()->getProgram(device(), "plane").program);
    prepared.getResource()->compiled.clear();
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuLifetimeTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuLifetimeTest);

} // namespace haylen::gpu
