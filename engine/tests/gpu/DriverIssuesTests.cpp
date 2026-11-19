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

// Only the swapchain of the Adreno 610 up to the driver that waited without end whatever timeout it got waits for its images without a timeout, while newer drivers and other GPUs keep the bounded wait.
TEST(DriverIssuesTest, AcquiresWithoutATimeoutOnlyOnTheAdreno610DriverThatIgnoredIt) {
    EXPECT_TRUE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "Adreno (TM) 610", .versionNumber = 0x8019F000}, Issue::UnboundedAcquire).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "Adreno (TM) 610", .versionNumber = 0x8019F001}, Issue::UnboundedAcquire).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "Adreno (TM) 630", .versionNumber = 0x8014B000}, Issue::UnboundedAcquire).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "Adreno (TM) 610", .versionNumber = 0x8019F000}, Issue::UnboundedAcquire).has_value());
}

// Only the OpenGL ES driver 25.10 of AMD, which refuses the disjoint state of the timer queries it names, reads none, while Mesa on the same GPU and other drivers read it.
TEST(DriverIssuesTest, ReadsNoDisjointStateOnlyOnTheOpenGlEsDriverOfAmdThatRefusesIt) {
    EXPECT_TRUE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "AMD Radeon Pro V620 MxGPU", .version = "OpenGL ES 3.2.0 25.10.251024 "}, Issue::NoGpuDisjoint).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "AMD Radeon Pro V620 MxGPU (radeonsi, navi21, LLVM 20.1.2, DRM 3.57, 6.8.0-1071-azure)", .version = "OpenGL ES 3.2 Mesa 25.2.8"}, Issue::NoGpuDisjoint).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = "AMD Radeon Pro V620 MxGPU", .version = "4.6.0 Core Profile Context 25.10.251024"}, Issue::NoGpuDisjoint).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "Adreno (TM) 630", .version = "OpenGL ES 3.2 V@415.0"}, Issue::NoGpuDisjoint).has_value());
}

// The OpenGL driver of AMD compiles on the thread that asks for it, whatever its version, while Mesa on the same GPU, its OpenGL ES and other drivers keep their compiler threads.
TEST(DriverIssuesTest, CompilesOnTheCallingThreadOnlyOnTheOpenGlDriverOfAmd) {
    EXPECT_TRUE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = "AMD Radeon Pro V620 MxGPU", .version = "4.6.0 Core Profile Context 25.10.251024"}, Issue::CompileOnCallingThread).has_value());
    EXPECT_TRUE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = "AMD Radeon RX 6800 XT", .version = "4.6.0 Core Profile Context 25.1.1.250115"}, Issue::CompileOnCallingThread).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = "AMD Radeon Pro V620 MxGPU (radeonsi, navi21, LLVM 20.1.2, DRM 3.57, 6.8.0-1071-azure)", .version = "4.6 (Core Profile) Mesa 25.2.8"}, Issue::CompileOnCallingThread).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "AMD Radeon Pro V620 MxGPU", .version = "OpenGL ES 3.2.0 25.10.251024"}, Issue::CompileOnCallingThread).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = "NVIDIA GeForce MX110/PCIe/SSE2", .version = "4.6.0 NVIDIA 596.86"}, Issue::CompileOnCallingThread).has_value());
}

// Only the OpenGL ES translator of the Android emulator on SwiftShader 4.0.0, whose clears reach only the first layer, clears later layers by drawing, while the translator on ANGLE and other drivers clear them.
TEST(DriverIssuesTest, ClearsLayersByDrawingOnlyOnTheTranslatorOfTheEmulatorOnSwiftShader) {
    const DriverIssues::Driver translator{.api = Device::Api::OpenGlEs, .renderer = "Android Emulator OpenGL ES Translator (Google SwiftShader)", .version = "OpenGL ES 3.0 (OpenGL ES 3.0 SwiftShader 4.0.0.1)"};
    EXPECT_TRUE(DriverIssues::find(translator, Issue::ClearLayersByDrawing).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "Android Emulator OpenGL ES Translator (ANGLE (Google, Vulkan 1.3.0 (SwiftShader Device (LLVM 10.0.0) (0x0000C0DE)), SwiftShader driver-5.0.0))", .version = "OpenGL ES 3.1 (OpenGL ES 3.1.0 (ANGLE 2.1.1 git hash: fbf66f49c7cc))"}, Issue::ClearLayersByDrawing).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = translator.renderer, .version = translator.version}, Issue::ClearLayersByDrawing).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGlEs, .renderer = "Adreno (TM) 830", .version = "OpenGL ES 3.2 V@0800.37"}, Issue::ClearLayersByDrawing).has_value());
}

// Only the OpenGL of llvmpipe up to Mesa 26.0, whose anisotropic filtering ignores the slopes of a lookup, filters without anisotropy, while newer Mesa, lavapipe and other drivers keep it.
TEST(DriverIssuesTest, FiltersWithoutAnisotropyOnlyOnTheLlvmpipeThatIgnoresTheSlopesOfALookup) {
    const std::string_view renderer = "llvmpipe (LLVM 21.1.8, 256 bits)";
    const std::string_view seen = "4.5 (Core Profile) Mesa 26.0.8-1ubuntu0.3";
    const std::string_view fixed = "4.5 (Core Profile) Mesa 26.1.0-devel (git-56997723)";
    EXPECT_TRUE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = renderer, .version = seen, .versionNumber = DriverIssues::readMesaVersion(seen)}, Issue::NoAnisotropy).has_value());
    EXPECT_TRUE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = "llvmpipe (LLVM 19.1.1, 128 bits)", .version = "4.5 (Core Profile) Mesa 25.2.8", .versionNumber = DriverIssues::readMesaVersion("4.5 (Core Profile) Mesa 25.2.8")}, Issue::NoAnisotropy).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = renderer, .version = fixed, .versionNumber = DriverIssues::readMesaVersion(fixed)}, Issue::NoAnisotropy).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::Vulkan, .renderer = "llvmpipe (LLVM 21.1.8, 256 bits)", .versionNumber = (26U << 22U) | 8U}, Issue::NoAnisotropy).has_value());
    EXPECT_FALSE(DriverIssues::find({.api = Device::Api::OpenGl, .renderer = "AMD Radeon Pro V620 MxGPU (radeonsi, navi21, LLVM 20.1.2, DRM 3.57, 6.8.0-1071-azure)", .version = "4.6 (Core Profile) Mesa 25.2.8"}, Issue::NoAnisotropy).has_value());
}

// The version of Mesa reads from the version text of its OpenGL and OpenGL ES as Mesa numbers its Vulkan drivers, and the text of another driver reads as 0.
TEST(DriverIssuesTest, ReadsTheVersionOfMesaFromTheVersionText) {
    EXPECT_EQ(DriverIssues::readMesaVersion("4.5 (Core Profile) Mesa 26.0.8-1ubuntu0.3"), (26U << 22U) | 8U);
    EXPECT_EQ(DriverIssues::readMesaVersion("OpenGL ES 3.2 Mesa 25.2.8"), (25U << 22U) | (2U << 12U) | 8U);
    EXPECT_EQ(DriverIssues::readMesaVersion("4.6 (Core Profile) Mesa 26.1-devel"), (26U << 22U) | (1U << 12U));
    EXPECT_EQ(DriverIssues::readMesaVersion("4.6.0 NVIDIA 596.86"), 0U);
    EXPECT_EQ(DriverIssues::readMesaVersion("4.5 Mesa x"), 0U);
    EXPECT_EQ(DriverIssues::readMesaVersion("4.5 Mesa 99999999999999999999.1"), 0U);
}

} // namespace haylen::gpu
