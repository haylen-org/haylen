#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>

#include "gpu/direct3d/WindowSamples.hpp"

namespace haylen::gpu::direct3d {

TEST(WindowSamplesTest, TakesTheCountsOfTheGpu) {
    for (const int samples : {1, 2, 4}) {
        EXPECT_NO_THROW(WindowSamples::check(Surface{.sampleCount = samples}, 1 | 2 | 4, "Direct3D 11"));
    }

    EXPECT_NO_THROW(WindowSamples::check(std::nullopt, 1, "Direct3D 11"));
}

// Direct3D would refuse such a swapchain with an error code that names no sample count.
TEST(WindowSamplesTest, RefusesACountTheGpuCannotTakeWithASentence) {
    for (const int samples : {0, 2, 3, 8}) {
        try {
            WindowSamples::check(Surface{.sampleCount = samples}, 1 | 4, "Direct3D 12");
            ADD_FAILURE() << "The window of " << samples << " samples was taken.";
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(std::string(error.what()), "The window asks for " + std::to_string(samples) + " samples, and Direct3D 12 draws a window on this GPU with 1 or 4 samples.");
        }
    }
}

// The swapchain draws its color and its depth with one count, so a window takes only the counts that both formats take.
TEST(WindowSamplesTest, TakesTheCountsOfBothTheColorAndTheDepthOfTheSwapchain) {
    Device::Capabilities capabilities;
    capabilities.sampleCounts[static_cast<std::size_t>(Device::PixelFormat::Bgra8)] = 1 | 2 | 4 | 8;
    capabilities.sampleCounts[static_cast<std::size_t>(Device::PixelFormat::Depth24Stencil8)] = 1 | 4;
    EXPECT_EQ(WindowSamples::getSwapchainCounts(capabilities), 1 | 4);
}

} // namespace haylen::gpu::direct3d
