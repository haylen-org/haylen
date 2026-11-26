#pragma once

#include <d3d12.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "gpu/d3d12/ReadbackCopy.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::d3d12 {

// The copy of a render target into a buffer of a readback heap, at rows of the pitch, whose pixels `poll` converts once the GPU finished the copy.
class ImageReadback final : public Device::Readback {
  public:
    ImageReadback(ReadbackCopy copy, UINT pitch, Device::PixelFormat pixelFormat, int imageWidth, int imageHeight);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    ReadbackCopy readback;
    UINT rowPitch = 0;
    Device::PixelFormat format = Device::PixelFormat::Rgba8;
    int width = 0;
    int height = 0;
    bool delivered = false;
};

} // namespace haylen::gpu::d3d12
