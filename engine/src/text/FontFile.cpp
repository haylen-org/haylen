#include "text/FontFile.hpp"

#include <algorithm>
#include <cstdlib>
#include <new>
#include <utility>
#include <vector>

#include "text/GlyphTables.hpp"

namespace haylen::text {

std::optional<FontFile> FontFile::read(std::span<const std::uint8_t> bytes, unsigned int face) {
    if (!std::in_range<unsigned int>(bytes.size())) {
        return std::nullopt;
    }
    const auto* const contents = reinterpret_cast<const char*>(bytes.data());
    const auto length = static_cast<unsigned int>(bytes.size());
    return open(Blob(hb_blob_create(contents, length, HB_MEMORY_MODE_READONLY, nullptr, nullptr), &hb_blob_destroy), length, face);
}

std::optional<FontFile> FontFile::read(std::vector<std::uint8_t> bytes, unsigned int face) {
    if (!std::in_range<unsigned int>(bytes.size())) {
        return std::nullopt;
    }
    auto owned = std::make_unique<std::vector<std::uint8_t>>(std::move(bytes));
    const auto* const contents = reinterpret_cast<const char*>(owned->data());
    const auto length = static_cast<unsigned int>(owned->size());
    return open(Blob(hb_blob_create(contents, length, HB_MEMORY_MODE_READONLY, owned.release(), &FontFile::release), &hb_blob_destroy), length, face);
}

unsigned int FontFile::countFaces(std::span<const std::uint8_t> bytes) {
    if (!std::in_range<unsigned int>(bytes.size())) {
        return 0;
    }
    const auto* const contents = reinterpret_cast<const char*>(bytes.data());
    const auto length = static_cast<unsigned int>(bytes.size());
    const Blob blob(hb_blob_create(contents, length, HB_MEMORY_MODE_READONLY, nullptr, nullptr), &hb_blob_destroy);
    if (hb_blob_get_length(blob.get()) != length) {
        throw std::bad_alloc();
    }
    return hb_face_count(blob.get());
}

void FontFile::release(void* owned) {
    delete static_cast<std::vector<std::uint8_t>*>(owned);
}

// HarfBuzz hands out an empty blob when it cannot allocate one, which would read as a file without a font, and the empty face for an index past the faces of the blob.
std::optional<FontFile> FontFile::open(Blob fileBlob, unsigned int length, unsigned int index) {
    if (hb_blob_get_length(fileBlob.get()) != length) {
        throw std::bad_alloc();
    }
    if (index >= hb_face_count(fileBlob.get())) {
        return std::nullopt;
    }
    FontFile file(std::move(fileBlob), index);
    if (!file.isUsable()) {
        return std::nullopt;
    }
    return file;
}

// HarfBuzz positions in font units once the scale of the font is its em square.
FontFile::FontFile(Blob fileBlob, unsigned int index) : blob(std::move(fileBlob)), face(hb_face_create(blob.get(), index), &hb_face_destroy), font(nullptr, &hb_font_destroy) {
    unsigned int length = 0;
    const char* contents = hb_blob_get_data(blob.get(), &length);
    bytes = {reinterpret_cast<const std::uint8_t*>(contents), length};
    unitsPerEm = hb_face_get_upem(face.get());
    glyphCount = hb_face_get_glyph_count(face.get());

    font.reset(hb_font_create(face.get()));
    hb_font_set_scale(font.get(), static_cast<int>(unitsPerEm), static_cast<int>(unitsPerEm));
    hb_font_extents_t extents{};
    hb_font_get_h_extents(font.get(), &extents);
    lineMetrics = {.ascender = extents.ascender, .descender = extents.descender, .lineGap = extents.line_gap};
}

bool FontFile::hasTable(hb_tag_t tag) const {
    const std::unique_ptr<hb_blob_t, decltype(&hb_blob_destroy)> table(hb_face_reference_table(face.get(), tag), &hb_blob_destroy);
    return hb_blob_get_length(table.get()) > 0;
}

bool FontFile::isUsable() const {
    const bool hasOutlines = hasTable(HB_TAG('g', 'l', 'y', 'f')) || hasTable(HB_TAG('C', 'F', 'F', ' ')) || hasTable(HB_TAG('C', 'F', 'F', '2'));
    return glyphCount > 0 && hasTable(HB_TAG('c', 'm', 'a', 'p')) && hasOutlines && lineMetrics.ascender > lineMetrics.descender;
}

bool FontFile::isDrawable() const {
    const unsigned int total = hb_face_get_table_tags(face.get(), 0, nullptr, nullptr);
    std::vector<hb_tag_t> tags(total);
    unsigned int count = total;
    hb_face_get_table_tags(face.get(), 0, &count, tags.data());
    return GlyphTables::isDrawable(std::span<const std::uint32_t>(tags.data(), count));
}

std::uint32_t FontFile::findGlyph(char32_t codePoint) const noexcept {
    hb_codepoint_t glyph = 0;
    return hb_font_get_nominal_glyph(font.get(), static_cast<hb_codepoint_t>(codePoint), &glyph) ? getHeldGlyph(glyph) : 0;
}

int FontFile::getAdvance(std::uint32_t glyph) const noexcept {
    return hb_font_get_glyph_h_advance(font.get(), glyph);
}

std::optional<hb_glyph_extents_t> FontFile::getExtents(std::uint32_t glyph) const noexcept {
    hb_glyph_extents_t extents{};
    if (!hb_font_get_glyph_extents(font.get(), glyph, &extents) || (extents.width == 0 && extents.height == 0)) {
        return std::nullopt;
    }

    // The box is measured in 64 bits, since its corners may lie past the range of the numbers HarfBuzz gives.
    const std::int64_t left = extents.x_bearing;
    const std::int64_t top = extents.y_bearing;
    const std::int64_t right = left + extents.width;
    const std::int64_t bottom = top + extents.height;
    const std::int64_t reach = std::max({std::abs(left), std::abs(top), std::abs(right), std::abs(bottom)});
    if (reach > kMaxReach) {
        return std::nullopt;
    }
    return extents;
}

} // namespace haylen::text
