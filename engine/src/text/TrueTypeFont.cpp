#include "haylen/text/TrueTypeFont.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>

#include <hb.h>

#include "haylen/graphics/Device.hpp"
#include "text/DistanceField.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace haylen::text {

// The parsed font file, sized by its em square at the bake size. The stb_truetype library reads the outlines and HarfBuzz shapes in font units, with a buffer kept for every run.
struct TrueTypeFont::Face {
    std::vector<std::uint8_t> ttf;
    stbtt_fontinfo info{};
    float scale = 1.0F;
    std::unique_ptr<hb_blob_t, decltype(&hb_blob_destroy)> blob{nullptr, &hb_blob_destroy};
    std::unique_ptr<hb_face_t, decltype(&hb_face_destroy)> shapingFace{nullptr, &hb_face_destroy};
    std::unique_ptr<hb_font_t, decltype(&hb_font_destroy)> shapingFont{nullptr, &hb_font_destroy};
    std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> buffer{hb_buffer_create(), &hb_buffer_destroy};
};

const TrueTypeFont::Options TrueTypeFont::kDefaultOptions{};

// A glyph larger than the largest texture could never be packed, so the bake size and the spread share the limit of the atlas.
const TrueTypeFont::Options& TrueTypeFont::validate(const Options& value, const graphics::Device& graphicsDevice) {
    const int limit = graphicsDevice.getMaxTextureSize();
    const std::string bound = "positive and at most the maximum texture size of " + std::to_string(limit) + ".";
    if (!(value.bakeSize > 0.0F && value.bakeSize <= static_cast<float>(limit))) {
        throw std::invalid_argument("The bake size of a TrueType font must be " + bound);
    }
    if (value.spread <= 0 || value.spread > limit) {
        throw std::invalid_argument("The spread of a TrueType font must be " + bound);
    }
    if (value.atlasSize <= 0 || value.atlasSize > limit) {
        throw std::invalid_argument("The atlas size of a TrueType font must be " + bound);
    }
    return value;
}

// The stb_truetype reader finds tables through the table directory without checking their bounds, so the directory and every table it lists must lie inside the file.
bool TrueTypeFont::hasTables(std::span<const std::uint8_t> ttf, std::size_t start) noexcept {
    // clang-format off
    const auto read = [&ttf](std::size_t offset, std::size_t size) {
        std::uint64_t value = 0;
        for (std::size_t index = 0; index < size; ++index) {
            value = value << 8U | static_cast<std::uint64_t>(ttf[offset + index]);
        }
        return value;
    };
    // clang-format on

    if (start > ttf.size() || ttf.size() - start < kOffsetTableSize) {
        return false;
    }
    const auto count = static_cast<std::size_t>(read(start + 4, 2));
    if ((ttf.size() - start - kOffsetTableSize) / kTableRecordSize < count) {
        return false;
    }
    for (std::size_t record = start + kOffsetTableSize; record < start + kOffsetTableSize + count * kTableRecordSize; record += kTableRecordSize) {
        if (read(record + 8, 4) + read(record + 12, 4) > ttf.size()) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<TrueTypeFont::Face> TrueTypeFont::open(std::vector<std::uint8_t> ttf, const Options& fontOptions) {
    auto opened = std::make_unique<Face>();
    opened->ttf = std::move(ttf);
    const int offset = opened->ttf.size() < kHeaderSize ? -1 : stbtt_GetFontOffsetForIndex(opened->ttf.data(), 0);
    if (offset < 0 || !hasTables(opened->ttf, static_cast<std::size_t>(offset)) || stbtt_InitFont(&opened->info, opened->ttf.data(), offset) == 0) {
        throw std::runtime_error("Font data is not a valid TrueType or OpenType font.");
    }
    opened->scale = stbtt_ScaleForMappingEmToPixels(&opened->info, fontOptions.bakeSize);

    // HarfBuzz reads the same bytes in place and positions glyphs in font units, which the scale of the em square turns into pixels.
    opened->blob.reset(hb_blob_create(reinterpret_cast<const char*>(opened->ttf.data()), static_cast<unsigned int>(opened->ttf.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr));
    opened->shapingFace.reset(hb_face_create(opened->blob.get(), 0));
    opened->shapingFont.reset(hb_font_create(opened->shapingFace.get()));
    const auto units = static_cast<int>(hb_face_get_upem(opened->shapingFace.get()));
    hb_font_set_scale(opened->shapingFont.get(), units, units);
    return opened;
}

Font::Metrics TrueTypeFont::readMetrics(const Face& opened, const Options& fontOptions) noexcept {
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&opened.info, &ascent, &descent, &lineGap);
    return {
        .nativeSize = fontOptions.bakeSize,
        .ascent = static_cast<float>(ascent) * opened.scale,
        .lineHeight = static_cast<float>(ascent - descent + lineGap) * opened.scale,
        .spread = static_cast<float>(fontOptions.spread),
    };
}

TrueTypeFont::TrueTypeFont(graphics::Device& graphicsDevice, std::vector<std::uint8_t> ttf, const Options& fontOptions) : TrueTypeFont(graphicsDevice, open(std::move(ttf), validate(fontOptions, graphicsDevice)), fontOptions) {}

TrueTypeFont::TrueTypeFont(graphics::Device& graphicsDevice, std::unique_ptr<Face> opened, const Options& fontOptions) : Font(readMetrics(*opened, fontOptions)), face(std::move(opened)), device(graphicsDevice), options(fontOptions) {
    atlasWidth = options.atlasSize;
    atlasHeight = options.atlasSize;
    atlas.assign(static_cast<std::size_t>(atlasWidth) * static_cast<std::size_t>(atlasHeight), 0);
    texture = device.createDynamicAlphaTexture(atlasWidth, atlasHeight, atlas, {.filter = graphics::Texture::Filter::Linear});
}

TrueTypeFont::~TrueTypeFont() = default;

std::span<const std::uint8_t> TrueTypeFont::getData() const noexcept {
    return face->ttf;
}

const graphics::Texture& TrueTypeFont::getPage(std::size_t index) const {
    if (index != 0) {
        throw std::out_of_range("A TrueType font has a single page.");
    }
    return texture;
}

bool TrueTypeFont::hasGlyph(char32_t codePoint) {
    const auto [entry, inserted] = coverage.try_emplace(codePoint, false);
    if (inserted) {
        entry->second = stbtt_FindGlyphIndex(&face->info, static_cast<int>(codePoint)) != 0;
    }
    return entry->second;
}

// HarfBuzz keeps the context around the run for the joining of its ends, and its default cluster level keeps every mark in the cluster of its letter. Without a language it applies no language-specific forms.
void TrueTypeFont::shape(const Run& run, std::vector<ShapedGlyph>& shaped) {
    hb_buffer_t* buffer = face->buffer.get();
    hb_buffer_clear_contents(buffer);
    hb_buffer_add_utf32(buffer, reinterpret_cast<const std::uint32_t*>(run.text.data()), static_cast<int>(run.text.size()), static_cast<unsigned int>(run.begin), static_cast<int>(run.end - run.begin));
    hb_buffer_set_direction(buffer, run.rightToLeft ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, hb_script_from_iso15924_tag(run.script));
    if (!run.language.empty()) {
        hb_buffer_set_language(buffer, hb_language_from_string(run.language.data(), static_cast<int>(run.language.size())));
    }
    hb_shape(face->shapingFont.get(), buffer, nullptr, 0);

    unsigned int count = 0;
    const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &count);
    const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, nullptr);
    shaped.reserve(shaped.size() + count);
    for (unsigned int index = 0; index < count; ++index) {
        const hb_glyph_position_t& position = positions[index];
        shaped.push_back({
            .index = infos[index].codepoint,
            .cluster = infos[index].cluster,
            .advance = static_cast<float>(position.x_advance) * face->scale,
            .offset = {static_cast<float>(position.x_offset) * face->scale, static_cast<float>(-position.y_offset) * face->scale},
        });
    }
}

// A glyph joins the cache only once it is in the atlas, so a glyph that did not fit fails again rather than drawing nothing.
const Font::Glyph& TrueTypeFont::getGlyph(std::uint32_t index) {
    if (const auto found = glyphs.find(index); found != glyphs.end()) {
        return found->second;
    }
    if (index >= static_cast<std::uint32_t>(face->info.numGlyphs)) {
        throw std::out_of_range("The font has no glyph with index " + std::to_string(index) + ".");
    }
    return glyphs.emplace(index, rasterize(index)).first->second;
}

Font::Glyph TrueTypeFont::rasterize(std::uint32_t index) {
    int advance = 0;
    int bearing = 0;
    stbtt_GetGlyphHMetrics(&face->info, static_cast<int>(index), &advance, &bearing);
    Glyph glyph{.advance = static_cast<float>(advance) * face->scale};

    // A glyph that no atlas could hold, with its pixel of padding on each side, fails before its field is built, which takes long at such sizes.
    const std::optional<DistanceField> box = DistanceField::measure(face->info, static_cast<int>(index), face->scale, options.spread);
    const int largest = device.getMaxTextureSize() - 2;
    if (box && (box->width > largest || box->height > largest)) {
        throw std::runtime_error("The glyph is larger than the largest font atlas the device can hold.");
    }

    const std::optional<DistanceField> field = DistanceField::build(face->info, static_cast<int>(index), face->scale, options.spread);
    if (!field) {
        return glyph;
    }
    const int width = field->width;
    const int height = field->height;

    // Glyphs are packed in shelves. A glyph that fits neither the current shelf nor a new one doubles the atlas.
    while (cursorX + width + 1 > atlasWidth || cursorY + height + 1 > atlasHeight) {
        const bool nextShelfFits = cursorX > 1 && width + 2 <= atlasWidth && cursorY + rowHeight + height + 2 <= atlasHeight;
        if (cursorX + width + 1 > atlasWidth && nextShelfFits) {
            cursorX = 1;
            cursorY += rowHeight + 1;
            rowHeight = 0;
            continue;
        }
        grow();
    }

    for (int row = 0; row < height; ++row) {
        std::copy_n(field->pixels.begin() + static_cast<std::ptrdiff_t>(row * width), width, atlas.begin() + static_cast<std::ptrdiff_t>((cursorY + row) * atlasWidth + cursorX));
    }

    glyph.source = {static_cast<float>(cursorX), static_cast<float>(cursorY), static_cast<float>(width), static_cast<float>(height)};
    glyph.offset = {static_cast<float>(field->offsetX), static_cast<float>(field->offsetY)};
    glyph.visible = true;
    cursorX += width + 1;
    rowHeight = std::max(rowHeight, height);
    dirty = true;
    return glyph;
}

void TrueTypeFont::grow() {
    // Doubling the shorter side keeps the atlas square or twice as wide as tall, and existing glyphs keep their pixel positions.
    const bool wider = atlasWidth <= atlasHeight;
    const int width = wider ? atlasWidth * 2 : atlasWidth;
    const int height = wider ? atlasHeight : atlasHeight * 2;
    if (std::max(width, height) > device.getMaxTextureSize()) {
        throw std::runtime_error("The font atlas exceeded the maximum texture size.");
    }

    std::vector<std::uint8_t> grown(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    for (int row = 0; row < atlasHeight; ++row) {
        std::copy_n(atlas.begin() + static_cast<std::ptrdiff_t>(row * atlasWidth), atlasWidth, grown.begin() + static_cast<std::ptrdiff_t>(row * width));
    }
    atlas = std::move(grown);
    atlasWidth = width;
    atlasHeight = height;
    dirty = true;
}

// The atlas changes in place, so draws that add glyphs during a frame upload it once, while a grown atlas becomes a new texture that the draws already made keep.
void TrueTypeFont::sync() {
    if (!dirty) {
        return;
    }
    if (texture.getWidth() == atlasWidth && texture.getHeight() == atlasHeight) {
        device.updateTexture(texture, atlas);
    } else {
        texture = device.createDynamicAlphaTexture(atlasWidth, atlasHeight, atlas, {.filter = graphics::Texture::Filter::Linear});
    }
    dirty = false;
}

} // namespace haylen::text
