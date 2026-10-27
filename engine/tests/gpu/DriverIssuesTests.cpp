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

} // namespace haylen::gpu
