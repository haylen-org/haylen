#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace haylen::content {

// Splits files into chunks where their content says, with the `fastcdc-v1` algorithm: a gear hash over the bytes after the minimum size, a stricter cut condition before the target size and a looser one after it, and a cut at the maximum size. An insertion moves only the chunks around it, and every chunk after them keeps its bytes and its ID. Files up to the target size stay one chunk, and chunks never cross from one file into another.
class Chunker final {
  public:
    static constexpr std::size_t kMinimumSize = 256 * 1024;
    static constexpr std::size_t kTargetSize = 1024 * 1024;
    static constexpr std::size_t kMaximumSize = 4 * 1024 * 1024;

    // Returns the length of the chunk that starts a window, which holds the next bytes of a file: at least `kMaximumSize` of them, or all that remain of the file.
    [[nodiscard]] static std::size_t findCut(std::span<const std::uint8_t> window) noexcept;

    [[nodiscard]] static bool isSingleChunk(std::uint64_t fileSize) noexcept {
        return fileSize <= kTargetSize;
    }

  private:
    // The cut conditions test the high bits of the hash, which depend on the last 64 bytes, two bits more before the target size and two fewer after it.
    static constexpr std::uint64_t kSmallMask = ~std::uint64_t{0} << (64 - 22);
    static constexpr std::uint64_t kLargeMask = ~std::uint64_t{0} << (64 - 18);
    static constexpr std::uint64_t kGearSeed = 0x6861796c656e7631;

    // The gear table holds 256 values of SplitMix64 from a fixed seed, pinned by the test vectors of the chunker.
    [[nodiscard]] static constexpr std::array<std::uint64_t, 256> makeGear() noexcept {
        std::array<std::uint64_t, 256> gear{};
        std::uint64_t state = kGearSeed;
        for (std::uint64_t& value : gear) {
            state += 0x9E3779B97F4A7C15;
            std::uint64_t mixed = state;
            mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9;
            mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EB;
            value = mixed ^ (mixed >> 31);
        }
        return gear;
    }
};

} // namespace haylen::content
