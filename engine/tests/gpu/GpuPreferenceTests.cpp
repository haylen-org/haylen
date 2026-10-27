#include <gtest/gtest.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "gpu/GpuPreference.hpp"

namespace haylen::gpu {

// The drivers of NVIDIA and AMD read the GPU that a process wants from the variables its executable exports, which the test program exports like the player and the apps, and the factory sets them to the GPU the app wants before its first context of OpenGL opens.
TEST(GpuPreferenceTest, TurnsTheVariablesOfTheDriversToTheGpuTheAppWants) {
#if defined(_WIN32)
    const HMODULE executable = GetModuleHandleW(nullptr);
    const auto* nvidia = reinterpret_cast<const DWORD*>(reinterpret_cast<void*>(GetProcAddress(executable, "NvOptimusEnablement")));
    const auto* amd = reinterpret_cast<const int*>(reinterpret_cast<void*>(GetProcAddress(executable, "AmdPowerXpressRequestHighPerformance")));
    ASSERT_NE(nvidia, nullptr);
    ASSERT_NE(amd, nullptr);

    GpuPreference::request(true);
    EXPECT_EQ(*nvidia, 0U);
    EXPECT_EQ(*amd, 0);

    GpuPreference::request(false);
    EXPECT_EQ(*nvidia, 1U);
    EXPECT_EQ(*amd, 1);
#else
    GpuPreference::request(true);
    GTEST_SKIP() << "Only the drivers of Windows read the GPU that a process wants from its executable.";
#endif
}

} // namespace haylen::gpu
