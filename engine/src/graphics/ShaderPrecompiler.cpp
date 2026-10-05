#include "graphics/ShaderPrecompiler.hpp"

namespace haylen::graphics {

// OpenGL, Direct3D 11 and WebGPU compile a program when it is made, on the thread that owns the device.
void ShaderPrecompiler::start(core::JobSystem&, std::vector<std::string>) {}

} // namespace haylen::graphics
