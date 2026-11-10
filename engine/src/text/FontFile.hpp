#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <hb.h>

namespace haylen::text {

// A TrueType or OpenType font file read by HarfBuzz, which checks every table, offset and count of the file before it uses them, so a font from any package is safe to read. Its font measures and draws in font units, the units of the em square of the file.
class FontFile final {
  public:
    // The heights of a line in font units, with the descender below the baseline negative.
    struct LineMetrics {
        int ascender = 0;
        int descender = 0;
        int lineGap = 0;
    };

    // No coordinate of a font file reaches this many font units from the origin of its glyph, so a box that does comes from a broken file.
    static constexpr int kMaxReach = 1 << 16;

    // Reads the face of the bytes that stay where they are while the file and every font made from it live. The face picks a font of a collection, a `.ttc` file, by its index from 0, and a file of one font has only the face 0. Returns nothing for bytes without that face or whose face is not a font with glyphs, a character map and outlines whose ascender rises above its descender.
    [[nodiscard]] static std::optional<FontFile> read(std::span<const std::uint8_t> bytes, unsigned int face = 0);

    // Reads the face of bytes that the file takes along, which live while HarfBuzz holds any font made from them.
    [[nodiscard]] static std::optional<FontFile> read(std::vector<std::uint8_t> bytes, unsigned int face = 0);

    // Returns how many faces the bytes hold: the fonts of a collection, 1 for a font file and 0 for bytes that are neither.
    [[nodiscard]] static unsigned int countFaces(std::span<const std::uint8_t> bytes);

    [[nodiscard]] std::span<const std::uint8_t> getBytes() const noexcept {
        return bytes;
    }
    [[nodiscard]] hb_face_t& getFace() const noexcept {
        return *face;
    }
    [[nodiscard]] hb_font_t& getFont() const noexcept {
        return *font;
    }
    [[nodiscard]] unsigned int getUnitsPerEm() const noexcept {
        return unitsPerEm;
    }
    [[nodiscard]] unsigned int getGlyphCount() const noexcept {
        return glyphCount;
    }
    [[nodiscard]] const LineMetrics& getLineMetrics() const noexcept {
        return lineMetrics;
    }

    // Returns the glyph that the character map gives a code point, or 0, the glyph of missing characters, which a glyph past the glyphs of the file is too.
    [[nodiscard]] std::uint32_t findGlyph(char32_t codePoint) const noexcept;

    // Returns the glyph, or 0, the glyph of missing characters, when the file does not hold it, as a character map or a substitution of a broken file may name.
    [[nodiscard]] std::uint32_t getHeldGlyph(std::uint32_t glyph) const noexcept {
        return glyph < glyphCount ? glyph : 0;
    }

    [[nodiscard]] int getAdvance(std::uint32_t glyph) const noexcept;

    // Returns the box of the outline of a glyph in font units, or nothing for a glyph without one, such as a space, or with a box that reaches past `kMaxReach`.
    [[nodiscard]] std::optional<hb_glyph_extents_t> getExtents(std::uint32_t glyph) const noexcept;

    // Whether the engine draws the glyphs of the face, as `GlyphTables::isDrawable` decides from its tables.
    [[nodiscard]] bool isDrawable() const;

  private:
    using Blob = std::unique_ptr<hb_blob_t, decltype(&hb_blob_destroy)>;

    FontFile(Blob fileBlob, unsigned int index);

    [[nodiscard]] static std::optional<FontFile> open(Blob fileBlob, unsigned int length, unsigned int index);
    static void release(void* owned);

    [[nodiscard]] bool hasTable(hb_tag_t tag) const;
    [[nodiscard]] bool isUsable() const;

    Blob blob;
    std::unique_ptr<hb_face_t, decltype(&hb_face_destroy)> face;
    std::unique_ptr<hb_font_t, decltype(&hb_font_destroy)> font;
    std::span<const std::uint8_t> bytes;
    unsigned int unitsPerEm = 0;
    unsigned int glyphCount = 0;
    LineMetrics lineMetrics;
};

} // namespace haylen::text
