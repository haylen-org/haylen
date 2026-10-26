#pragma once

#import <Metal/Metal.h>

#include <array>
#include <atomic>
#include <cstddef>
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
    [[nodiscard]] static float fromHalf(std::uint16_t value) noexcept;
    [[nodiscard]] static std::array<std::uint8_t, 4> toBytes(const std::array<float, 4>& color) noexcept;
    [[nodiscard]] static std::array<float, 4> readHalves(const std::byte* source, std::size_t channels) noexcept;
    [[nodiscard]] static std::array<float, 4> readFloats(const std::byte* source, std::size_t channels) noexcept;
    [[nodiscard]] std::array<std::uint8_t, 4> readPixel(const std::byte* source) const noexcept;

    id<MTLBuffer> buffer = nil;
    std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
    gpu::Device::PixelFormat format = gpu::Device::PixelFormat::Rgba8;
    std::size_t pixelCount = 0;
};

} // namespace haylen::gpu::metal
