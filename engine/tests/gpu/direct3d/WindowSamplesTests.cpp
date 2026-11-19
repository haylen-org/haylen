#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>

#include "gpu/direct3d/WindowSamples.hpp"

namespace haylen::gpu::direct3d {

TEST(WindowSamplesTest, TakesPowersOfTwoUpToTheMostOfTheGpu) {
    for (const int samples : {1, 2, 4}) {
        EXPECT_NO_THROW(WindowSamples::check(Surface{.sampleCount = samples}, 4, "Direct3D 11"));
    }

    EXPECT_NO_THROW(WindowSamples::check(std::nullopt, 4, "Direct3D 11"));
}

// Direct3D would refuse such a swapchain with an error code that names no sample count.
TEST(WindowSamplesTest, RefusesACountTheGpuCannotTakeWithASentence) {
    for (const int samples : {0, 3, 8}) {
        try {
            WindowSamples::check(Surface{.sampleCount = samples}, 4, "Direct3D 12");
            ADD_FAILURE() << "The window of " << samples << " samples was taken.";
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(std::string(error.what()), "The window asks for " + std::to_string(samples) + " samples, and Direct3D 12 draws a window on this GPU with 1, 2, 4 or 8 samples, up to 4.");
        }
    }
}

} // namespace haylen::gpu::direct3d
