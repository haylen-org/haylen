#pragma once

namespace haylen::gpu {

// Tells the drivers of a laptop with two GPUs which GPU the process wants, an integrated one that saves power or a discrete one that draws faster. The drivers of NVIDIA and AMD read it from the variables `NvOptimusEnablement` and `AmdPowerXpressRequestHighPerformance` that the executable exports, which `GpuPreferenceExports.cpp` defines, and pick the GPU of OpenGL from them when its first context opens. Direct3D and Vulkan list every GPU and take the one that their options ask for, so only OpenGL needs this. An executable without the variables leaves the choice to the drivers and the settings of the system.
class GpuPreference final {
  public:
    // Asks for the integrated GPU when `lowPower` is on and for the discrete one otherwise, before the first context of OpenGL opens.
    static void request(bool lowPower) noexcept;
};

} // namespace haylen::gpu
