#include "haylen/text/TrueTypeFont.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <string>

#include <hb-ot.h>
#include <hb.h>

#include "haylen/graphics/Device.hpp"
#include "text/ColorGlyphImage.hpp"
#include "text/DistanceField.hpp"
#include "text/FontFile.hpp"

namespace haylen::text {

// The font file, sized by its em square at the bake size. HarfBuzz reads its outlines and metrics, shapes in font units with a buffer kept for every run and paints color glyphs. The blob of the file owns its bytes, so the images of color glyphs keep them alive after the font.
struct TrueTypeFont::Face {
    // The features that a run without ligatures turns off: the standard and contextual ligatures and the contextual alternates, which programming fonts draw their ligatures with. The required ligatures of a script stay, since its text needs them.
    static constexpr std::array<hb_feature_t, 3> kWithoutLigatures{{
        {.tag = HB_TAG('l', 'i', 'g', 'a'), .value = 0, .start = HB_FEATURE_GLOBAL_START, .end = HB_FEATURE_GLOBAL_END},
        {.tag = HB_TAG('c', 'l', 'i', 'g'), .value = 0, .start = HB_FEATURE_GLOBAL_START, .end = HB_FEATURE_GLOBAL_END},
        {.tag = HB_TAG('c', 'a', 'l', 't'), .value = 0, .start = HB_FEATURE_GLOBAL_START, .end = HB_FEATURE_GLOBAL_END},
    }};

    FontFile file;
    float scale = 1.0F;
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

std::unique_ptr<TrueTypeFont::Face> TrueTypeFont::open(std::vector<std::uint8_t> ttf, const Options& fontOptions) {
    std::optional<FontFile> file = FontFile::read(std::move(ttf));
    if (!file) {
        throw std::runtime_error("Font data is not a valid TrueType or OpenType font.");
    }
    const float scale = fontOptions.bakeSize / static_cast<float>(file->getUnitsPerEm());
    return std::make_unique<Face>(Face{.file = std::move(*file), .scale = scale});
}

// The capital H reaches the cap height, which is the ascent in a font without one.
Font::Metrics TrueTypeFont::readMetrics(const Face& opened, const Options& fontOptions) {
    const FontFile& file = opened.file;
    const FontFile::LineMetrics& lines = file.getLineMetrics();
    int top = lines.ascender;
    if (const std::uint32_t capital = file.findGlyph('H'); capital != 0) {
        if (const std::optional<hb_glyph_extents_t> box = file.getExtents(capital)) {
            top = box->y_bearing;
        }
    }
    const std::uint32_t letterO = file.findGlyph('o');
    return {
        .nativeSize = fontOptions.bakeSize,
        .ascent = static_cast<float>(lines.ascender) * opened.scale,
        .lineHeight = static_cast<float>(lines.ascender - lines.descender + lines.lineGap) * opened.scale,
        .capHeight = static_cast<float>(top) * opened.scale,
        .spread = static_cast<float>(fontOptions.spread),
        .counter = letterO != 0 ? DistanceField::measureHole(file, letterO, opened.scale) : 0.0F,
    };
}

TrueTypeFont::TrueTypeFont(graphics::Device& graphicsDevice, std::vector<std::uint8_t> ttf, const Options& fontOptions) : TrueTypeFont(graphicsDevice, open(std::move(ttf), validate(fontOptions, graphicsDevice)), fontOptions) {}

TrueTypeFont::TrueTypeFont(graphics::Device& graphicsDevice, std::unique_ptr<Face> opened, const Options& fontOptions) : Font(readMetrics(*opened, fontOptions)), face(std::move(opened)), device(graphicsDevice), options(fontOptions) {
    atlasWidth = options.atlasSize;
    atlasHeight = options.atlasSize;
    atlas.assign(static_cast<std::size_t>(atlasWidth) * static_cast<std::size_t>(atlasHeight), 0);
    texture = device.createDynamicDistanceTexture(atlasWidth, atlasHeight, atlas, {.filter = graphics::Texture::Filter::Linear});
}

TrueTypeFont::~TrueTypeFont() = default;

std::span<const std::uint8_t> TrueTypeFont::getData() const noexcept {
    return face->file.getBytes();
}

const graphics::Texture& TrueTypeFont::getPage(std::size_t index) const {
    if (index != 0) {
        throw std::out_of_range("A TrueType font has a single page.");
    }
    return texture;
}

TrueTypeFont::Coverage TrueTypeFont::cover(char32_t codePoint) {
    const auto [entry, inserted] = coverage.try_emplace(codePoint, Coverage::Missing);
    if (!inserted) {
        return entry->second;
    }
    const std::uint32_t index = face->file.findGlyph(codePoint);
    if (index != 0) {
        entry->second = isColor(index) ? Coverage::Color : Coverage::Outline;
    }
    return entry->second;
}

bool TrueTypeFont::hasGlyph(char32_t codePoint) {
    return cover(codePoint) != Coverage::Missing;
}

bool TrueTypeFont::hasColorGlyph(char32_t codePoint) {
    return cover(codePoint) == Coverage::Color;
}

// A glyph has colors when the `COLR` table paints it, through a paint graph of version 1 or the layers of version 0.
bool TrueTypeFont::isColor(std::uint32_t index) const noexcept {
    hb_face_t* shaping = &face->file.getFace();
    return hb_ot_color_glyph_has_paint(shaping, index) || hb_ot_color_glyph_get_layers(shaping, index, 0, nullptr, nullptr) > 0;
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
    if (run.ligatures) {
        hb_shape(&face->file.getFont(), buffer, nullptr, 0);
    } else {
        hb_shape(&face->file.getFont(), buffer, Face::kWithoutLigatures.data(), static_cast<unsigned int>(Face::kWithoutLigatures.size()));
    }

    unsigned int count = 0;
    const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &count);
    const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, nullptr);
    shaped.reserve(shaped.size() + count);
    for (unsigned int index = 0; index < count; ++index) {
        const hb_glyph_position_t& position = positions[index];
        shaped.push_back({
            .index = face->file.getHeldGlyph(infos[index].codepoint),
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
    if (index >= face->file.getGlyphCount()) {
        throw std::out_of_range("The font has no glyph with index " + std::to_string(index) + ".");
    }
    return glyphs.emplace(index, rasterize(index)).first->second;
}

Font::Glyph TrueTypeFont::rasterize(std::uint32_t index) {
    Glyph glyph{.advance = static_cast<float>(face->file.getAdvance(index)) * face->scale};
    if (isColor(index)) {
        return paintColors(glyph, index);
    }

    // A glyph that no atlas could hold, with its pixel of padding on each side, fails before its field is built.
    const std::optional<DistanceField> field = DistanceField::build(face->file, index, face->scale, options.spread, device.getMaxTextureSize() - 2);
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
        std::copy_n(field->distances.begin() + static_cast<std::ptrdiff_t>(row * width), width, atlas.begin() + static_cast<std::ptrdiff_t>((cursorY + row) * atlasWidth + cursorX));
    }

    glyph.source = {static_cast<float>(cursorX), static_cast<float>(cursorY), static_cast<float>(width), static_cast<float>(height)};
    glyph.offset = {static_cast<float>(field->offsetX), static_cast<float>(field->offsetY)};
    glyph.visible = true;
    cursorX += width + 1;
    rowHeight = std::max(rowHeight, height);
    dirty = true;
    return glyph;
}

// A color glyph draws its own image instead of a distance field, so it takes no room in the atlas. Its box is the extent of what its layers paint.
Font::Glyph TrueTypeFont::paintColors(Glyph glyph, std::uint32_t index) const {
    hb_glyph_extents_t extents{};
    if (!hb_font_get_glyph_extents(&face->file.getFont(), index, &extents) || extents.width <= 0 || extents.height >= 0) {
        return glyph;
    }
    glyph.image = graphics::VectorImage(std::make_shared<ColorGlyphImage>(face->file.getFont(), index, extents, face->scale));
    glyph.source = {0.0F, 0.0F, glyph.image.getSize().x, glyph.image.getSize().y};
    glyph.offset = {static_cast<float>(extents.x_bearing) * face->scale, static_cast<float>(-extents.y_bearing) * face->scale};
    glyph.visible = true;
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

    std::vector<std::uint16_t> grown(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
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
        device.updateTexture(texture, std::span(reinterpret_cast<const std::uint8_t*>(atlas.data()), atlas.size() * sizeof(std::uint16_t)));
    } else {
        texture = device.createDynamicDistanceTexture(atlasWidth, atlasHeight, atlas, {.filter = graphics::Texture::Filter::Linear});
    }
    dirty = false;
}

} // namespace haylen::text
