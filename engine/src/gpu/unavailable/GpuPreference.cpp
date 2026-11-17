#include "gpu/GpuPreference.hpp"

namespace haylen::gpu {

// Only the drivers of Windows read the GPU that a process wants from the variables of its executable, and the other systems pick the GPU of a laptop with two through their own settings.
void GpuPreference::request(bool) noexcept {}

} // namespace haylen::gpu
