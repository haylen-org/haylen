#include "backends/sokol/Precompiler.hpp"

namespace haylen::sokol {

// OpenGL, Direct3D 11 and WebGPU compile a program when it is made, on the thread that owns the device.
void Precompiler::start(core::JobSystem&, std::vector<std::string>) {}

} // namespace haylen::sokol
