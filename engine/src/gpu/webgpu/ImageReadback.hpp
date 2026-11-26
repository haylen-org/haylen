#pragma once

#include <webgpu/webgpu.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "gpu/webgpu/ReadbackBuffer.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::webgpu {

// A copy of a render target into a buffer that the CPU maps once the GPU finished it, without waiting for it. The rows of the copy are as wide as WebGPU aligns them, and the pixels come out as four bytes of red, green, blue and alpha whatever the format of the image, with no channel the format lacks but alpha, which is full.
class ImageReadback final : public Device::Readback {
  public:
    ImageReadback(WGPUDevice device, WGPUQueue queue, WGPUTexture texture, Device::PixelFormat pixelFormat, int pixelsWide, int pixelsHigh);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    static constexpr std::uint32_t kRowAlignment = 256;

    // The bytes of a row of the copy, which throws `ContractError` of the rule `Readback` for a depth format, whose pixels WebGPU copies into no buffer of colors.
    [[nodiscard]] static std::uint32_t measureRow(Device::PixelFormat format, int width);

    Device::PixelFormat format;
    int width;
    int height;
    std::uint32_t rowBytes;
    ReadbackBuffer copy;
};

} // namespace haylen::gpu::webgpu
