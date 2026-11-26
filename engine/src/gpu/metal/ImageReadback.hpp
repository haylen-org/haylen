#pragma once

#import <Metal/Metal.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::metal {

// A copy of the top left pixels of a render target into a buffer the CPU reads, which a blit of its own command buffer makes after the frames committed before it, and whose completion handler marks it done. The pixels come as four bytes of red, green, blue and alpha from the top row, whatever the format of the render target.
class ImageReadback final : public gpu::Device::Readback {
  public:
    ImageReadback(id<MTLCommandQueue> queue, id<MTLTexture> texture, gpu::Device::PixelFormat format, int width, int height);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    id<MTLBuffer> buffer = nil;
    std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
    gpu::Device::PixelFormat format = gpu::Device::PixelFormat::Rgba8;
    int width = 0;
    int height = 0;
    bool delivered = false;
};

} // namespace haylen::gpu::metal
