#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace haylen::navigation3d {

// Compresses the voxels of a tile layer, its heights, areas and connections one after the other, with Zstandard, and reads them back.
class LayerCodec final {
  public:
    [[nodiscard]] static std::vector<std::uint8_t> compress(std::span<const std::uint8_t> heights, std::span<const std::uint8_t> areas, std::span<const std::uint8_t> connections);

    // Fills `voxels` with exactly `size` bytes, and returns false for compressed bytes that do not hold exactly that many.
    [[nodiscard]] static bool decompress(std::span<const std::uint8_t> compressed, std::size_t size, std::vector<std::uint8_t>& voxels);

  private:
    static constexpr int kLevel = 3;
};

} // namespace haylen::navigation3d
