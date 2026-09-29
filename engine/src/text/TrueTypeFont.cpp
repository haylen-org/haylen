#include "haylen/text/TrueTypeFont.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/graphics/Device.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace haylen::text {

struct TrueTypeFont::Face {
    std::vector<std::uint8_t> ttf;
    stbtt_fontinfo info{};
    float scale = 1.0F;
};

const TrueTypeFont::Options TrueTypeFont::kDefaultOptions{};

std::unique_ptr<TrueTypeFont::Face> TrueTypeFont::open(std::vector<std::uint8_t> ttf, const Options& fontOptions) {
    auto opened = std::make_unique<Face>();
    opened->ttf = std::move(ttf);
    const int offset = opened->ttf.size() < kHeaderSize ? -1 : stbtt_GetFontOffsetForIndex(opened->ttf.data(), 0);
    if (offset < 0 || stbtt_InitFont(&opened->info, opened->ttf.data(), offset) == 0) {
        throw std::runtime_error("Font data is not a valid TrueType or OpenType font.");
    }
    opened->scale = stbtt_ScaleForPixelHeight(&opened->info, fontOptions.bakeSize);
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

TrueTypeFont::TrueTypeFont(graphics::Device& graphicsDevice, std::vector<std::uint8_t> ttf, const Options& fontOptions) : TrueTypeFont(graphicsDevice, open(std::move(ttf), fontOptions), fontOptions) {}

TrueTypeFont::TrueTypeFont(graphics::Device& graphicsDevice, std::unique_ptr<Face> opened, const Options& fontOptions) : Font(readMetrics(*opened, fontOptions)), face(std::move(opened)), device(graphicsDevice), options(fontOptions) {
    atlasWidth = options.atlasSize;
    atlasHeight = options.atlasSize;
    atlas.assign(static_cast<std::size_t>(atlasWidth * atlasHeight), 0);
    texture = device.createAlphaTexture(atlasWidth, atlasHeight, atlas, {.filter = graphics::Texture::Filter::Linear});
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

const Font::Glyph& TrueTypeFont::getGlyph(char32_t codePoint) {
    if (const auto found = glyphs.find(codePoint); found != glyphs.end()) {
        return found->second;
    }

    Glyph& created = glyphs[codePoint];
    rasterize(codePoint, created);
    return created;
}

float TrueTypeFont::getKerning(char32_t left, char32_t right) {
    return static_cast<float>(stbtt_GetCodepointKernAdvance(&face->info, static_cast<int>(left), static_cast<int>(right))) * face->scale;
}

void TrueTypeFont::rasterize(char32_t codePoint, Glyph& glyph) {
    const int character = static_cast<int>(codePoint);
    int advance = 0;
    int bearing = 0;
    stbtt_GetCodepointHMetrics(&face->info, character, &advance, &bearing);
    glyph.advance = static_cast<float>(advance) * face->scale;

    int width = 0;
    int height = 0;
    int offsetX = 0;
    int offsetY = 0;
    const float distanceScale = 128.0F / static_cast<float>(options.spread);
    unsigned char* bitmap = stbtt_GetCodepointSDF(&face->info, face->scale, character, options.spread, 128, distanceScale, &width, &height, &offsetX, &offsetY);
    if (bitmap == nullptr) {
        return;
    }

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
        std::copy_n(bitmap + row * width, width, atlas.begin() + static_cast<std::ptrdiff_t>((cursorY + row) * atlasWidth + cursorX));
    }
    stbtt_FreeSDF(bitmap, nullptr);

    glyph.source = {static_cast<float>(cursorX), static_cast<float>(cursorY), static_cast<float>(width), static_cast<float>(height)};
    glyph.offset = {static_cast<float>(offsetX), static_cast<float>(offsetY)};
    glyph.visible = true;
    cursorX += width + 1;
    rowHeight = std::max(rowHeight, height);
    dirty = true;
}

void TrueTypeFont::grow() {
    // Doubling the shorter side keeps the atlas square or twice as wide as tall, and existing glyphs keep their pixel positions.
    const bool wider = atlasWidth <= atlasHeight;
    const int width = wider ? atlasWidth * 2 : atlasWidth;
    const int height = wider ? atlasHeight : atlasHeight * 2;
    if (std::max(width, height) > device.getMaxTextureSize()) {
        throw std::runtime_error("The font atlas exceeded the maximum texture size.");
    }

    std::vector<std::uint8_t> grown(static_cast<std::size_t>(width * height), 0);
    for (int row = 0; row < atlasHeight; ++row) {
        std::copy_n(atlas.begin() + static_cast<std::ptrdiff_t>(row * atlasWidth), atlasWidth, grown.begin() + static_cast<std::ptrdiff_t>(row * width));
    }
    atlas = std::move(grown);
    atlasWidth = width;
    atlasHeight = height;
    dirty = true;
}

void TrueTypeFont::sync() {
    if (!dirty) {
        return;
    }
    device.replaceAlphaTexture(texture, atlasWidth, atlasHeight, atlas);
    dirty = false;
}

} // namespace haylen::text
