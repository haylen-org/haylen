#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "sokol_gfx.h"

namespace haylen::graphics {

// Copies the pixels of a color image of RGBA bytes to memory the CPU reads, through the API of the backend the build chose, without waiting for the GPU: Metal blits into a shared buffer in `GpuReadback.mm`, and elsewhere Direct3D 11 copies into a staging texture, OpenGL and WebGL2 read into a pixel buffer behind a fence and WebGPU copies into a buffer it maps. The dummy backend of tests answers with transparent pixels.
class GpuReadback final {
  public:
    // Starts the copy, which must come after the passes that render the image are committed.
    GpuReadback(sg_image image, int imageWidth, int imageHeight);
    ~GpuReadback();

    GpuReadback(const GpuReadback&) = delete;
    GpuReadback& operator=(const GpuReadback&) = delete;

    // Returns the rows from the first one the backend stores, four bytes a pixel, once the copy finished, and nothing while the GPU works.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll();

  private:
    // The objects of the backend that the copy uses until it finishes.
    struct Pending;

    int width = 0;
    int height = 0;
    std::unique_ptr<Pending> pending;
};

} // namespace haylen::graphics
