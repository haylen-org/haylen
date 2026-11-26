#pragma once

#import <Metal/Metal.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "gpu/metal/ReadbackCopy.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::metal {

// A copy of the top left pixels of a render target into a buffer the CPU reads, after the frames committed before it. The pixels come as four bytes of red, green, blue and alpha from the top row, whatever the format of the render target.
class ImageReadback final : public gpu::Device::Readback {
  public:
    ImageReadback(id<MTLCommandQueue> queue, id<MTLTexture> texture, gpu::Device::PixelFormat format, int width, int height);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    ReadbackCopy copy;
    gpu::Device::PixelFormat format = gpu::Device::PixelFormat::Rgba8;
    int width = 0;
    int height = 0;
};

} // namespace haylen::gpu::metal
