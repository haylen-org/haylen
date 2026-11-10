#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace haylen::text {

// Decides from the tags of the tables of a face whether the engine draws its glyphs: through the color layers of `COLR`, or through outlines in `glyf`, `CFF ` or `CFF2` without the bitmaps of `sbix` or `CBDT`, which hold the real glyphs of the faces that have them, such as many emoji fonts, whose outlines are empty.
class GlyphTables final {
  public:
    [[nodiscard]] static bool isDrawable(std::span<const std::uint32_t> tags) noexcept {
        const bool layered = std::ranges::find(tags, kColorLayers) != tags.end();
        const bool outlined = std::ranges::find_first_of(tags, kOutlineTables) != tags.end();
        const bool bitmapped = std::ranges::find_first_of(tags, kBitmapTables) != tags.end();
        return layered || (outlined && !bitmapped);
    }

  private:
    static constexpr std::array<std::uint32_t, 3> kOutlineTables{0x676C7966U, 0x43464620U, 0x43464632U};
    static constexpr std::array<std::uint32_t, 2> kBitmapTables{0x73626978U, 0x43424454U};
    static constexpr std::uint32_t kColorLayers = 0x434F4C52U;
};

} // namespace haylen::text
