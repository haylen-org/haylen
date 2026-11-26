#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "gpu/d3d11/StagingCopy.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::d3d11 {

// The copy of a render target into a staging texture, whose pixels `poll` converts once the GPU finished the copy.
class ImageReadback final : public Device::Readback {
  public:
    ImageReadback(StagingCopy copy, Device::PixelFormat pixelFormat, int imageWidth, int imageHeight);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    StagingCopy staging;
    Device::PixelFormat format = Device::PixelFormat::Rgba8;
    int width = 0;
    int height = 0;
    bool delivered = false;
};

} // namespace haylen::gpu::d3d11
