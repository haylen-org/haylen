#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "sokol_gfx.h"

namespace haylen::sokol {

// Copies the pixels of a color image of RGBA bytes to memory the CPU reads, through the graphics API the build chose, without waiting for the GPU: Metal blits into a shared buffer in `ImageReadback.mm`, and elsewhere Direct3D 11 copies into a staging texture, OpenGL and WebGL2 read into a pixel buffer behind a fence and WebGPU copies into a buffer it maps.
class ImageReadback final : public gpu::Device::Readback {
  public:
    // Starts the copy, which must come after the passes that render the image are committed.
    ImageReadback(sg_image image, int imageWidth, int imageHeight);
    ~ImageReadback() override;

    ImageReadback(const ImageReadback&) = delete;
    ImageReadback& operator=(const ImageReadback&) = delete;

    // Returns the rows from the first one the backend stores, four bytes a pixel, once the copy finished, and nothing while the GPU works.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    // The objects of the backend that the copy uses until it finishes.
    struct Pending;

    int width = 0;
    int height = 0;
    std::unique_ptr<Pending> pending;
};

} // namespace haylen::sokol
