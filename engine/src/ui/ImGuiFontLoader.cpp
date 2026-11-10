#include "ui/ImGuiFontLoader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <new>
#include <optional>
#include <utility>

namespace haylen::ui {

const ImFontLoader& ImGuiFontLoader::get() {
    static const ImFontLoader loader = make();
    return loader;
}

ImFontLoader ImGuiFontLoader::make() {
    ImFontLoader loader;
    loader.Name = "harfbuzz";
    loader.FontSrcInit = &initSource;
    loader.FontSrcDestroy = &destroySource;
    loader.FontSrcGetGlyphIndexFromCodepoint = &findGlyph;
    loader.FontSrcContainsGlyph = &containsGlyph;
    loader.FontBakedInit = &initBaked;
    loader.FontBakedLoadGlyph = &loadGlyph;
    return loader;
}

// ImGui sizes a font by its height from ascent to descent, and a source merged into a font of another size scales by the ratio of the sizes and by its own extra scale.
bool ImGuiFontLoader::initSource(ImFontAtlas* /*atlas*/, ImFontConfig* source) {
    const std::span<const std::uint8_t> bytes(static_cast<const std::uint8_t*>(source->FontData), static_cast<std::size_t>(source->FontDataSize));
    std::optional<text::FontFile> file = text::FontFile::read(bytes, static_cast<unsigned int>(source->FontNo));
    if (!file) {
        return false;
    }

    const text::FontFile::LineMetrics& lines = file->getLineMetrics();
    const float referenceSize = source->DstFont->Sources[0]->SizePixels;
    if (source->MergeMode && source->SizePixels == 0.0F) {
        source->SizePixels = referenceSize;
    }
    float scale = 1.0F / static_cast<float>(lines.ascender - lines.descender);
    if (source->MergeMode && source->SizePixels != 0.0F && referenceSize != 0.0F) {
        scale *= source->SizePixels / referenceSize;
    }
    scale *= source->ExtraSizeScale;

    auto loaded = std::make_unique<Source>(Source{.file = std::move(*file), .scale = scale});
    loaded->raster.reset(hb_raster_draw_create_or_fail());
    if (!loaded->raster) {
        throw std::bad_alloc();
    }
    source->FontLoaderData = loaded.release();
    return true;
}

void ImGuiFontLoader::destroySource(ImFontAtlas* /*atlas*/, ImFontConfig* source) {
    const std::unique_ptr<Source> loaded(static_cast<Source*>(source->FontLoaderData));
    source->FontLoaderData = nullptr;
}

int ImGuiFontLoader::findGlyph(ImFontAtlas* /*atlas*/, ImFontConfig* source, ImWchar codePoint) {
    const Source& loaded = *static_cast<const Source*>(source->FontLoaderData);
    return static_cast<int>(loaded.file.findGlyph(static_cast<char32_t>(codePoint)));
}

bool ImGuiFontLoader::containsGlyph(ImFontAtlas* /*atlas*/, ImFontConfig* source, int glyph) {
    const Source& loaded = *static_cast<const Source*>(source->FontLoaderData);
    return glyph > 0 && static_cast<unsigned int>(glyph) < loaded.file.getGlyphCount();
}

bool ImGuiFontLoader::initBaked(ImFontAtlas* /*atlas*/, ImFontConfig* source, ImFontBaked* baked, void* /*loaderData*/) {
    if (source->MergeMode) {
        return true;
    }
    const Source& loaded = *static_cast<const Source*>(source->FontLoaderData);
    const text::FontFile::LineMetrics& lines = loaded.file.getLineMetrics();
    const float layoutScale = loaded.scale * baked->Size / source->ExtraSizeScale;
    baked->Ascent = ImCeil(static_cast<float>(lines.ascender) * layoutScale);
    baked->Descent = ImFloor(static_cast<float>(lines.descender) * layoutScale);
    return true;
}

// A glyph that is too large for the atlas loads as missing, so ImGui draws the fallback glyph in its place.
bool ImGuiFontLoader::loadGlyph(ImFontAtlas* atlas, ImFontConfig* source, ImFontBaked* baked, void* /*loaderData*/, int glyph, ImFontGlyph* loaded, float* advance) {
    Source& font = *static_cast<Source*>(source->FontLoaderData);
    const auto index = static_cast<std::uint32_t>(glyph);
    int oversampleX = 0;
    int oversampleY = 0;
    ImFontAtlasBuildGetOversampleFactors(source, baked, &oversampleX, &oversampleY);
    const float layoutScale = font.scale * baked->Size;
    const float density = source->RasterizerDensity * baked->RasterizerDensity;
    const float scaleX = font.scale * baked->Size * density * static_cast<float>(oversampleX);
    const float scaleY = font.scale * baked->Size * density * static_cast<float>(oversampleY);
    const float advanceX = static_cast<float>(font.file.getAdvance(index)) * layoutScale;
    if (advance != nullptr) {
        *advance = advanceX;
        return true;
    }
    loaded->AdvanceX = advanceX;

    Box box;
    if (!measure(font, index, scaleX, scaleY, box)) {
        return false;
    }
    if (box.left == box.right || box.top == box.bottom) {
        return true;
    }
    const int width = box.right - box.left + oversampleX - 1;
    const int height = box.bottom - box.top + oversampleY - 1;
    if (width > atlas->TexMaxWidth || height > atlas->TexMaxHeight) {
        return false;
    }
    const ImFontAtlasRectId pack = ImFontAtlasPackAddRect(atlas, width, height);
    if (pack == ImFontAtlasRectId_Invalid) {
        return false;
    }
    ImTextureRect* rect = ImFontAtlasPackGetRect(atlas, pack);

    // The glyph fills the top left of the buffer, and the box filters of the oversampling spread it over the rest.
    ImVector<unsigned char>& buffer = atlas->Builder->TempBuffer;
    buffer.resize(width * height);
    const std::span<std::uint8_t> pixels(buffer.Data, static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    std::ranges::fill(pixels, std::uint8_t{0});
    rasterize(font, index, scaleX, scaleY, box, pixels, width);
    filterRows(pixels, width, height, oversampleX);
    filterColumns(pixels, width, height, oversampleY);

    const float referenceSize = baked->OwnerFont->Sources[0]->SizePixels;
    const float offsetScale = referenceSize != 0.0F ? baked->Size / referenceSize : 1.0F;
    const float offsetX = ImFloor(source->GlyphOffset.x * offsetScale + 0.5F) + getOversampleShift(oversampleX);
    const float baseline = getOversampleShift(oversampleY) + IM_ROUND(baked->Ascent);
    const float offsetY = ImFloor(source->GlyphOffset.y * offsetScale + 0.5F) + baseline;
    const float pixelWidth = 1.0F / (static_cast<float>(oversampleX) * density);
    const float pixelHeight = 1.0F / (static_cast<float>(oversampleY) * density);
    loaded->X0 = static_cast<float>(box.left) * pixelWidth + offsetX;
    loaded->Y0 = static_cast<float>(box.top) * pixelHeight + offsetY;
    loaded->X1 = static_cast<float>(box.left + static_cast<int>(rect->w)) * pixelWidth + offsetX;
    loaded->Y1 = static_cast<float>(box.top + static_cast<int>(rect->h)) * pixelHeight + offsetY;
    loaded->Visible = true;
    loaded->PackId = pack;
    ImFontAtlasBakedSetFontGlyphBitmap(atlas, baked, source, loaded, rect, pixels.data(), ImTextureFormat_Alpha8, width);
    return true;
}

// The box rounds the corners of the box of the outline out to whole pixels. It fails for a box whose corners lie farther from the origin than a pixel coordinate can hold.
bool ImGuiFontLoader::measure(const Source& source, std::uint32_t glyph, float scaleX, float scaleY, Box& box) {
    box = {};
    const std::optional<hb_glyph_extents_t> extents = source.file.getExtents(glyph);
    if (!extents) {
        return true;
    }

    const float left = std::floor(static_cast<float>(extents->x_bearing) * scaleX);
    const float top = std::floor(static_cast<float>(-extents->y_bearing) * scaleY);
    const float right = std::ceil(static_cast<float>(extents->x_bearing + extents->width) * scaleX);
    const float bottom = std::ceil(static_cast<float>(-(extents->y_bearing + extents->height)) * scaleY);
    if (std::max({std::fabs(left), std::fabs(top), std::fabs(right), std::fabs(bottom)}) > kMaxPixel) {
        return false;
    }
    box = {.left = static_cast<int>(left), .top = static_cast<int>(top), .right = static_cast<int>(right), .bottom = static_cast<int>(bottom)};
    return true;
}

// HarfBuzz draws in font units with y pointing up and keeps the rows of its image from the bottom up, so the transform scales into pixels and the rows turn over.
void ImGuiFontLoader::rasterize(Source& source, std::uint32_t glyph, float scaleX, float scaleY, const Box& box, std::span<std::uint8_t> pixels, int stride) {
    hb_raster_draw_t* raster = source.raster.get();
    const int width = box.right - box.left;
    const int height = box.bottom - box.top;
    hb_raster_draw_set_transform(raster, scaleX, 0.0F, 0.0F, scaleY, 0.0F, 0.0F);
    const hb_raster_extents_t area{.x_origin = box.left, .y_origin = -box.bottom, .width = static_cast<unsigned int>(width), .height = static_cast<unsigned int>(height), .stride = 0};
    hb_raster_draw_set_extents(raster, &area);
    hb_raster_draw_glyph(raster, &source.file.getFont(), glyph);
    std::unique_ptr<hb_raster_image_t, decltype(&hb_raster_image_destroy)> image(hb_raster_draw_render(raster), &hb_raster_image_destroy);
    if (!image) {
        throw std::bad_alloc();
    }

    hb_raster_extents_t rendered{};
    hb_raster_image_get_extents(image.get(), &rendered);
    const std::span<const std::uint8_t> coverage(hb_raster_image_get_buffer(image.get()), static_cast<std::size_t>(rendered.stride) * static_cast<std::size_t>(height));
    for (int row = 0; row < height; ++row) {
        const std::span<const std::uint8_t> from = coverage.subspan(static_cast<std::size_t>(height - 1 - row) * rendered.stride, static_cast<std::size_t>(width));
        std::ranges::copy(from, pixels.begin() + static_cast<std::ptrdiff_t>(row) * stride);
    }
    hb_raster_draw_recycle_image(raster, image.release());
}

// Each pixel takes the mean of itself and the pixels before it across the kernel, rounded down, which walking each row from its end computes in place.
void ImGuiFontLoader::filterRows(std::span<std::uint8_t> pixels, int width, int height, int kernel) {
    if (kernel <= 1) {
        return;
    }
    for (int row = 0; row < height; ++row) {
        const std::span<std::uint8_t> line = pixels.subspan(static_cast<std::size_t>(row) * static_cast<std::size_t>(width), static_cast<std::size_t>(width));
        unsigned int total = 0;
        for (int column = std::max(0, width - kernel); column < width; ++column) {
            total += line[static_cast<std::size_t>(column)];
        }
        for (int column = width - 1; column >= 0; --column) {
            const unsigned int value = line[static_cast<std::size_t>(column)];
            line[static_cast<std::size_t>(column)] = static_cast<std::uint8_t>(total / static_cast<unsigned int>(kernel));
            total -= value;
            if (column >= kernel) {
                total += line[static_cast<std::size_t>(column - kernel)];
            }
        }
    }
}

void ImGuiFontLoader::filterColumns(std::span<std::uint8_t> pixels, int width, int height, int kernel) {
    if (kernel <= 1) {
        return;
    }
    const auto at = [width](int row, int column) { return static_cast<std::size_t>(row) * static_cast<std::size_t>(width) + static_cast<std::size_t>(column); };
    for (int column = 0; column < width; ++column) {
        unsigned int total = 0;
        for (int row = std::max(0, height - kernel); row < height; ++row) {
            total += pixels[at(row, column)];
        }
        for (int row = height - 1; row >= 0; --row) {
            const unsigned int value = pixels[at(row, column)];
            pixels[at(row, column)] = static_cast<std::uint8_t>(total / static_cast<unsigned int>(kernel));
            total -= value;
            if (row >= kernel) {
                total += pixels[at(row - kernel, column)];
            }
        }
    }
}

// The box filter moves the glyph by half its width less one pixel of the oversampled raster, which this shift takes back.
float ImGuiFontLoader::getOversampleShift(int oversample) noexcept {
    return -static_cast<float>(oversample - 1) / (2.0F * static_cast<float>(oversample));
}

} // namespace haylen::ui
