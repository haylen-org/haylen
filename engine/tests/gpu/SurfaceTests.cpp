#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <string>

#include "gpu/support/GpuFixture.hpp"
#include "gpu/support/SecondSurface.hpp"

namespace haylen::gpu {

class GpuSurfaceTest : public test::GpuFixture {
  protected:
    void SetUp() override {
        GpuFixture::SetUp();
        if (IsSkipped() || HasFatalFailure()) {
            return;
        }
        try {
            first = std::make_unique<test::SecondSurface>(GetParam(), offscreen(), 64, 48);
            second = std::make_unique<test::SecondSurface>(GetParam(), offscreen(), 16, 16);
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
    void drawInto(int surface, math::Color color) {
        device().beginPass({.clear = color, .label = "haylen-test-surface", .surface = surface});
        device().applyPipeline(plane);
        drawPlane(color);
        device().endPass();
        device().beginPass({.load = Device::Load::Keep, .label = "haylen-test-surface-kept", .surface = surface});
        device().applyPipeline(plane);
        device().applyViewport({0.0F, 0.0F, 8.0F, 8.0F});
        drawPlane(math::Color::white());
        device().endPass();
    }

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

INSTANTIATE_TEST_SUITE_P(Devices, GpuSurfaceTest, ::testing::ValuesIn(test::GpuFixture::getTestedApis()), test::GpuFixture::nameApi);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GpuSurfaceTest);

} // namespace haylen::gpu
