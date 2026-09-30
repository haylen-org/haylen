#pragma once

#include <string>

namespace haylen::graphics {

// Names the GPU behind the device that sokol_gfx created, through the API of the backend the build chose. Apple platforms read it from Metal in GpuAdapter.mm and the others in GpuAdapter.cpp.
class GpuAdapter final {
  public:
    // Returns an empty name for a backend without an adapter to name, such as the dummy backend of tests.
    [[nodiscard]] static std::string getName();
};

} // namespace haylen::graphics
