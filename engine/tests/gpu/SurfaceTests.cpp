#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "gpu/support/GpuFixture.hpp"
#include "gpu/support/SecondSurface.hpp"
#include "support/ContractRule.hpp"

namespace haylen::gpu {

class GpuSurfaceTest : public test::GpuFixture {
  protected:
    void SetUp() override {
        GpuFixture::SetUp();

        if (IsSkipped() || HasFatalFailure()) {
            return;
        }

        try {
            first = std::make_unique<test::SecondSurface>(device(), offscreen(), 64, 48);
            second = std::make_unique<test::SecondSurface>(device(), offscreen(), 16, 16);
        } catch (const std::runtime_error& error) {
            GTEST_SKIP() << error.what();
        }

        const std::array<Device::ColorTarget, 1> formats{};
        plane = device().makePipeline({.program = program("plane"), .colors = formats, .swapchain = true, .label = "haylen-test-surface-plane"});
    }

    // The surfaces go after the device let go of them, and before the device itself.
    void TearDown() override {
        if (plane.isValid()) {
            device().destroy(plane);
        }

        GpuFixture::TearDown();
        first.reset();
        second.reset();
    }

    // Clears the swapchain of the surface and draws over it in a pass that keeps what the first one drew.
    void drawInto(int number, math::Color color) {
        device().beginPass({.clear = color, .label = "haylen-test-surface", .surface = number});
        device().applyPipeline(plane);
        drawPlane(color);
        device().endPass();
        device().beginPass({.load = Device::Load::Keep, .label = "haylen-test-surface-kept", .surface = number});
        device().applyPipeline(plane);
        device().applyViewport({0.0F, 0.0F, 8.0F, 8.0F});
        drawPlane(math::Color::white());
        device().endPass();
    }

    // The side of the target and the draws that make the GPU work of a frame far longer than its recording on the CPU, and the frames that play.
    static constexpr int kHeavySide = 1024;
    static constexpr int kHeavyDraws = 16;
    static constexpr int kPlayedFrames = 12;

    std::unique_ptr<test::SecondSurface> first;
    std::unique_ptr<test::SecondSurface> second;
    Device::Pipeline plane;
};

// The device draws into surfaces beyond the window through the pipelines of the swapchain, between passes into render targets, and follows each surface as it resizes and goes away, with the validation of its API watching.
TEST_P(GpuSurfaceTest, DrawsIntoSurfacesBeyondTheWindow) {
    int board = 0;
    try {
        board = device().addSurface(first->getSurface());
    } catch (const std::runtime_error& error) {
        // Windows makes no swapchain for a process without a desktop, such as one that a service or a remote shell starts.
        if (std::string(error.what()).find("no desktop") == std::string::npos) {
            throw;
        }

        GTEST_SKIP() << error.what();
    }

    EXPECT_GE(board, 1);

    for (int frame = 0; frame < 3; ++frame) {
        drawInto(board, {0.1F, 0.2F, 0.3F, 1.0F});
        device().commit();
    }

    // A surface that resizes takes its new size from the next frame, while a second surface draws in the same frames.
    device().resizeSurface(board, {.width = 32, .height = 24, .windowWidth = 32, .windowHeight = 24});
    const int side = device().addSurface(second->getSurface());
    EXPECT_NE(side, board);

    for (int frame = 0; frame < 3; ++frame) {
        drawInto(board, {0.3F, 0.2F, 0.1F, 1.0F});
        drawInto(side, {0.0F, 0.5F, 0.0F, 1.0F});
        device().commit();
    }

    const Pixels pixels = clearAndRead({0.2F, 0.4F, 0.6F, 0.8F}, 4, 4);
    EXPECT_TRUE(isNear(pixels.at(2, 2), {0.2F, 0.4F, 0.6F, 0.8F}));

    // A surface that went away leaves its number to the next one.
    device().removeSurface(board);
    EXPECT_EQ(device().addSurface(first->getSurface()), board);
    drawInto(board, {0.4F, 0.4F, 0.4F, 1.0F});
    device().commit();
    device().removeSurface(board);
    device().removeSurface(side);
    device().commit();
    EXPECT_FALSE(device().isLost());

    // Only a surface that "addSurface" made and that is still there goes away, never the window, which is the surface 0.
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().removeSurface(board); }), gpu::ContractError::Rule::Surface);
    EXPECT_EQ(test::ContractRule::of([&] { (void)device().removeSurface(0); }), gpu::ContractError::Rule::Surface);
}

// A surface comes between the passes of a frame, as the window of an output that opens while the app draws does, and takes the swapchain passes of that frame.
TEST_P(GpuSurfaceTest, AddsASurfaceBetweenThePassesOfAFrame) {
    const Device::Image target = makeTarget(4, 4);
    const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = math::Color::black()}}};
    device().beginPass({.colors = colors, .label = "haylen-test-before-surface"});
    device().endPass();

    int board = 0;
    try {
        board = device().addSurface(first->getSurface());
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("no desktop") == std::string::npos) {
            throw;
        }

        device().commit();
        GTEST_SKIP() << error.what();
    }

    drawInto(board, {0.1F, 0.2F, 0.3F, 1.0F});
    device().commit();
    device().removeSurface(board);
    device().commit();
    EXPECT_FALSE(device().isLost());
}

// A surface that resizes between the passes of a frame keeps the size of the frame until the next one, so the later passes of the frame draw and cut their scissors within the drawable they started with.
TEST_P(GpuSurfaceTest, KeepsTheSizeOfAFrameWhileItsSurfaceResizes) {
    int board = 0;
    try {
        board = device().addSurface(first->getSurface());
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("no desktop") == std::string::npos) {
            throw;
        }

        GTEST_SKIP() << error.what();
    }

    drawInto(board, {0.1F, 0.2F, 0.3F, 1.0F});
    device().commit();

    device().beginPass({.clear = math::Color::black(), .label = "haylen-test-before-resize", .surface = board});
    device().applyPipeline(plane);
    drawPlane(math::Color::white());
    device().endPass();
    device().resizeSurface(board, {.width = 128, .height = 96, .windowWidth = 128, .windowHeight = 96});
    device().beginPass({.load = Device::Load::Keep, .label = "haylen-test-after-resize", .surface = board});
    device().applyPipeline(plane);
    device().applyViewport({0.0F, 0.0F, 128.0F, 96.0F});
    device().applyScissor({0.0F, 0.0F, 128.0F, 96.0F});
    drawPlane(math::Color::white());
    device().endPass();
    device().commit();

    drawInto(board, {0.3F, 0.2F, 0.1F, 1.0F});
    device().commit();
    device().removeSurface(board);
    device().commit();
    EXPECT_FALSE(device().isLost());
}

// A drawable of fewer pixels than its window, as when the app caps its pixel density, takes the passes of each frame, which the system or the device stretches over the window, and follows the window as it resizes, scaled and not, with the validation of its API watching.
TEST_P(GpuSurfaceTest, StretchesADrawableSmallerThanItsWindow) {
    Surface scaled = first->getSurface();
    scaled.size = {.width = 32, .height = 24, .windowWidth = 64, .windowHeight = 48};
    int board = 0;
    try {
        board = device().addSurface(scaled);
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("no desktop") == std::string::npos) {
            throw;
        }

        GTEST_SKIP() << error.what();
    }

    for (int frame = 0; frame < 3; ++frame) {
        drawInto(board, {0.1F, 0.2F, 0.3F, 1.0F});
        device().commit();
    }

    device().resizeSurface(board, {.width = 21, .height = 16, .windowWidth = 64, .windowHeight = 48});

    for (int frame = 0; frame < 3; ++frame) {
        drawInto(board, {0.3F, 0.2F, 0.1F, 1.0F});
        device().commit();
    }

    device().resizeSurface(board, {.width = 64, .height = 48, .windowWidth = 64, .windowHeight = 48});

    for (int frame = 0; frame < 3; ++frame) {
        drawInto(board, {0.0F, 0.5F, 0.0F, 1.0F});
        device().commit();
    }

    device().resizeSurface(board, scaled.size);
    drawInto(board, {0.4F, 0.4F, 0.4F, 1.0F});
    device().commit();
    device().removeSurface(board);
    device().commit();
    EXPECT_FALSE(device().isLost());
}

// A platform reads the input of a frame once the wait for the display returns, so the age of that input on the screen counts the frames that the GPU still had to finish then. Each frame here gives the GPU far more work than it takes the CPU to record, as a frame that would let the CPU run ahead of the GPU does, and presents a surface. The copy of the target of each frame starts after its commit and comes before the next frame on the GPU. Once the wait after a frame returns, the GPU finished the frame before it, so the copy of the frame before that one finished too, and the input of the next frame waits behind one frame at most.
TEST_P(GpuSurfaceTest, ReadsTheInputOfAFrameBehindOneFrameAtMost) {
    if (GetParam() == Device::Api::WebGpu || GetParam() == Device::Api::WebGl2) {
        GTEST_SKIP() << "The browser paces the frames of a page by its animation frames, and a page may not wait for the GPU.";
    }

    int board = 0;
    try {
        board = device().addSurface(first->getSurface());
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("no desktop") == std::string::npos) {
            throw;
        }

        GTEST_SKIP() << error.what();
    }

    const Device::Image target = makeTarget(kHeavySide, kHeavySide);
    const std::array<Device::ColorTarget, 1> formats{};
    const Device::Pipeline heavy = makePlanePipeline(formats);
    const std::array<Device::ColorAttachment, 1> colors{{{.image = target, .clear = math::Color::black()}}};
    std::vector<std::unique_ptr<Device::Readback>> copies;

    for (int frame = 0; frame < kPlayedFrames; ++frame) {
        device().beginPass({.colors = colors, .label = "haylen-test-heavy"});
        device().applyPipeline(heavy);
        for (int draw = 0; draw < kHeavyDraws; ++draw) {
            drawPlane({0.1F, 0.2F, 0.3F, 1.0F});
        }

        device().endPass();
        drawInto(board, {0.3F, 0.2F, 0.1F, 1.0F});
        device().commit();
        copies.push_back(device().readImage(target, 4, 4));
        device().waitForDisplay();

        if (frame >= 2) {
            const std::optional<std::vector<std::uint8_t>> pixels = copies[static_cast<std::size_t>(frame - 2)]->poll();
            EXPECT_TRUE(pixels.has_value()) << "The GPU had not finished frame " << frame - 1 << " when frame " << frame + 1 << " read its input.";
        }
    }

    device().removeSurface(board);
    device().commit();
    device().waitForDisplay();
    EXPECT_FALSE(device().isLost());
}

INSTANTIATE_TEST_SUITE_P(Devices, GpuSurfaceTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuSurfaceTest);

} // namespace haylen::gpu
