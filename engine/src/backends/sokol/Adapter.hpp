#pragma once

#include <string>

namespace haylen::sokol {

// Names the GPU behind the device that sokol_gfx created, through the API of the backend the build chose. Apple platforms read it from Metal in `Adapter.mm` and the others in `Adapter.cpp`.
class Adapter final {
  public:
    // Returns an empty name where the graphics API names no adapter.
    [[nodiscard]] static std::string getName();
};

} // namespace haylen::sokol
