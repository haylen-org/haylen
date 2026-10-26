#pragma once

#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::webgpu {

// A copy of a render target into a buffer that the CPU maps once the GPU finished it, without waiting for it. The rows of the copy are as wide as WebGPU aligns them, and the pixels come out as four bytes of red, green, blue and alpha whatever the format of the image, with no channel the format lacks but alpha, which is full.
class ImageReadback final : public Device::Readback {
  public:
    ImageReadback(WGPUDevice device, WGPUQueue queue, WGPUTexture texture, Device::PixelFormat pixelFormat, int pixelsWide, int pixelsHigh);
    ImageReadback(const ImageReadback&) = delete;
    ImageReadback& operator=(const ImageReadback&) = delete;
    ~ImageReadback() override;

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    // Where the mapping stands, which the callback of the mapping holds until it runs, so it outlives a readback that ends first.
    enum class State : std::uint8_t {
        Mapping,
        Mapped,
        Failed,
    };

    static constexpr std::uint32_t kRowAlignment = 256;

    static void receiveMapping(WGPUMapAsyncStatus status, WGPUStringView message, void* first, void* second);
    [[nodiscard]] static float fromHalf(std::uint16_t bits) noexcept;
    [[nodiscard]] static std::uint8_t toByte(float value) noexcept;
    [[nodiscard]] static std::uint8_t toByte(std::uint32_t value, std::uint32_t maximum) noexcept;

    void convertRow(std::span<const std::byte> source, std::span<std::uint8_t> target) const noexcept;

    WGPUBuffer buffer = nullptr;
    Device::PixelFormat format;
    int width;
    int height;
    std::uint32_t rowBytes = 0;
    std::shared_ptr<State> state = std::make_shared<State>(State::Mapping);
};

} // namespace haylen::gpu::webgpu
