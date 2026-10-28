#include <gtest/gtest.h>

#include "gpu/DriverIssues.hpp"

namespace haylen::gpu {

using Issue = DriverIssues::Issue;

// The Vulkan driver of the PowerVR Rogue GE8320 that stopped inside pipeline creation is avoided up to its version, and a newer driver or another GPU starts.
TEST(DriverIssuesTest, AvoidsVulkanOnTheDriverOfThePowerVrRogueGe8320UpToItsVersion) {
    const DriverIssues::Driver seen{.api = Device::Api::Vulkan, .renderer = "PowerVR Rogue GE8320", .versionNumber = 5776728};
    EXPECT_TRUE(DriverIssues::find(seen, Issue::AvoidApi).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "PowerVR Rogue GE8320", .versionNumber = 5776729}, Issue::AvoidApi).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "PowerVR D-Series DXT-48-1536", .versionNumber = 1}, Issue::AvoidApi).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "PowerVR Rogue GE8320", .versionNumber = 5776728}, Issue::AvoidApi).has_value());
}

// The OpenGL ES of the Xclipse GPUs keeps its 32-bit float formats out of the filterable ones, whatever its build, while other GPUs keep them.
TEST(DriverIssuesTest, KeepsFloatFilteringFromTheOpenGlEsOfXclipseGpus) {
    const DriverIssues::Driver angle{.api = Device::Api::OpenGlEs, .renderer = "ANGLE (Samsung Xclipse 530) on Vulkan 1.3.231"};
    EXPECT_TRUE(DriverIssues::find(angle, Issue::NoFloatFiltering).has_value());
    EXPECT_FALSE(DriverIssues::find(angle, Issue::AvoidApi).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "Adreno (TM) 830"}, Issue::NoFloatFiltering).has_value());
}

// Only the OpenGL ES driver r19p0 of the Mali-G72, whose passes the GPU dropped with a reset of the context, flushes after each pass.
TEST(DriverIssuesTest, FlushesEachPassOnlyOnTheMaliG72DriverThatDroppedPasses) {
    const DriverIssues::Driver seen{.api = Device::Api::OpenGlEs, .renderer = "Mali-G72", .version = "OpenGL ES 3.2 v1.r19p0-01rel0.###other-sha0123456789ABCDEF0###"};
    EXPECT_TRUE(DriverIssues::find(seen, Issue::FlushEachPass).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "Mali-G72", .version = "OpenGL ES 3.2 v1.r38p1-01eac0"}, Issue::FlushEachPass).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "Mali-G72", .version = "OpenGL ES 3.2 v1.r19p0"}, Issue::FlushEachPass).has_value());
}

// Only the Vulkan driver of the Adreno 630 that read kept depth as cleared, up to its version, stores the depth that a pass keeps.
TEST(DriverIssuesTest, StoresKeptDepthOnlyOnTheAdreno630DriverThatLostIt) {
    EXPECT_TRUE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "Adreno (TM) 630", .versionNumber = 0x8014B000}, Issue::StoreKeptDepth).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "Adreno (TM) 630", .versionNumber = 0x8014B001}, Issue::StoreKeptDepth).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "Adreno (TM) 830", .versionNumber = 0x8014B000}, Issue::StoreKeptDepth).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "Adreno (TM) 630", .versionNumber = 0x8014B000}, Issue::StoreKeptDepth).has_value());
}

// Only the OpenGL ES driver 25.10 of AMD, which refuses the disjoint state of the timer queries it names, reads none, while Mesa on the same GPU and other drivers read it.
TEST(DriverIssuesTest, ReadsNoDisjointStateOnlyOnTheOpenGlEsDriverOfAmdThatRefusesIt) {
    EXPECT_TRUE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "AMD Radeon Pro V620 MxGPU", .version = "OpenGL ES 3.2.0 25.10.251024 "}, Issue::NoGpuDisjoint).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "AMD Radeon Pro V620 MxGPU (radeonsi, navi21, LLVM 20.1.2, DRM 3.57, 6.8.0-1071-azure)", .version = "OpenGL ES 3.2 Mesa 25.2.8"}, Issue::NoGpuDisjoint).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = "AMD Radeon Pro V620 MxGPU", .version = "4.6.0 Core Profile Context 25.10.251024"}, Issue::NoGpuDisjoint).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "Adreno (TM) 630", .version = "OpenGL ES 3.2 V@415.0"}, Issue::NoGpuDisjoint).has_value());
}

} // namespace haylen::gpu
