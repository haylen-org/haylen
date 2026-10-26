#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::direct3d {

// Turns the rows that a readback copies out of a render target into the four bytes a pixel that `Device::Readback` hands over, red, green, blue and alpha in that order. Floating point channels clamp to the range of a byte, and a channel the format lacks reads as a sampler reads it, zero for color and one for alpha.
class PixelConverter final {
  public:
    static void convertRow(Device::PixelFormat format, std::span<const std::byte> row, std::span<std::uint8_t> pixels) noexcept;

  private:
    using Channels = std::array<float, 4>;

    [[nodiscard]] static Channels read(Device::PixelFormat format, const std::byte* pixel) noexcept;
    [[nodiscard]] static float readHalf(const std::byte* bytes) noexcept;
    [[nodiscard]] static float readFloat(const std::byte* bytes) noexcept;
    [[nodiscard]] static std::uint8_t toByte(float value) noexcept;
};

} // namespace haylen::gpu::direct3d
