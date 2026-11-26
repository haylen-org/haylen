#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>

#include "3d/gltf/Diagnostics.hpp"

namespace haylen::gltf {

// Finds the chunks of a GLB file, the binary container of glTF: a header of twelve bytes with the magic `glTF`, the version 2 and the length of the file, then the JSON chunk and, second and at most once, the binary chunk. Other chunks are skipped. Every length is checked against the length the header declares and the bytes the file holds before anything reads them.
class GlbContainer final {
  public:
    static constexpr std::uint32_t kMagic = 0x46546C67U;
    static constexpr std::uint32_t kVersion = 2;
    static constexpr std::uint32_t kJsonChunk = 0x4E4F534AU;
    static constexpr std::uint32_t kBinaryChunk = 0x004E4942U;
    static constexpr std::uint64_t kHeaderSize = 12;
    static constexpr std::uint64_t kChunkHeaderSize = 8;

    // Where a chunk lies in the file.
    struct Range {
        std::uint64_t offset = 0;
        std::uint64_t length = 0;
    };

    struct Layout {
        Range json;
        std::optional<Range> binary;
    };

    // Fills the target with the bytes of the file at an offset, which the caller checked the file holds.
    using ReadAt = std::function<void(std::uint64_t, std::span<std::uint8_t>)>;

    // Tells whether bytes start with the magic of a GLB file.
    [[nodiscard]] static bool isGlb(std::span<const std::uint8_t> head) noexcept;

    [[nodiscard]] static Layout parse(std::uint64_t fileSize, const ReadAt& read, std::uint64_t maxJsonBytes, const Diagnostics& diagnostics);

  private:
    [[nodiscard]] static std::uint32_t readWord(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept;
};

} // namespace haylen::gltf
