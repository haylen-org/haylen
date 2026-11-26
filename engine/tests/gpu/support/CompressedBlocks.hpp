#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::test {

// One block of a block compressed format that every texel of it decodes to the same color, written by the rules of its family, with the color that a draw samples from it, so the GPU tests check the upload and the sampling of each compressed format against a known color. The colors lie near the color of the texture tests, quantized as each family stores them, and the sRGB formats sample their colors in linear values.
class CompressedBlocks final {
  public:
    struct Block {
        std::vector<std::byte> bytes;
        math::Color sampled;
    };

    CompressedBlocks() = delete;

    [[nodiscard]] static Block make(gpu::Device::PixelFormat format);

  private:
    // Writes bits from the lowest bit of the first byte up, the order of the BC6H and BC7 blocks.
    class BitWriter final {
      public:
        void write(std::uint32_t value, int count) noexcept;

        [[nodiscard]] const std::array<std::byte, 16>& getBytes() const noexcept {
            return bytes;
        }

      private:
        std::array<std::byte, 16> bytes{};
        int position = 0;
    };

    [[nodiscard]] static Block makeBc(gpu::Device::PixelFormat format);
    [[nodiscard]] static Block makeEtc(gpu::Device::PixelFormat format);
    [[nodiscard]] static Block makeAstc(gpu::Device::PixelFormat format);

    [[nodiscard]] static std::array<std::byte, 8> makeColorBlock(math::Color& sampled);
    [[nodiscard]] static std::array<std::byte, 8> makeChannelBlock(std::uint8_t value);
    [[nodiscard]] static std::array<std::byte, 8> makeSignedChannelBlock(std::int8_t value);
    [[nodiscard]] static std::array<std::byte, 16> makeBc6h(bool signedFloat, math::Color& sampled);
    [[nodiscard]] static std::array<std::byte, 16> makeBc7(math::Color& sampled);
    [[nodiscard]] static std::array<std::byte, 8> makeEtc2Color(math::Color& sampled);
    [[nodiscard]] static std::array<std::byte, 8> makeEacAlpha(std::uint8_t value);
    [[nodiscard]] static std::array<std::byte, 8> makeEac11(bool signedChannel, float target, float& sampled);

    [[nodiscard]] static int unquantizeBc6h(int value, bool signedFloat) noexcept;
    [[nodiscard]] static float fromHalf(std::uint16_t half) noexcept;
    [[nodiscard]] static std::uint16_t toHalf(float value) noexcept;
    [[nodiscard]] static int expand(int value, int bits) noexcept;
    [[nodiscard]] static bool isSrgb(gpu::Device::PixelFormat format) noexcept;

    static void append(std::vector<std::byte>& bytes, std::span<const std::byte> block);
};

} // namespace haylen::test
