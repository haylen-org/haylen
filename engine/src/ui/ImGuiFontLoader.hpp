#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include <hb-raster.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "text/FontFile.hpp"

namespace haylen::ui {

// The font loader of Dear ImGui, which reads every font through HarfBuzz, so the font files of any package are safe to hand to ImGui. It measures glyphs like the loader that ImGui ships, and HarfBuzz rasterizes them with the same oversampling and the same box filter.
class ImGuiFontLoader final {
  public:
    [[nodiscard]] static const ImFontLoader& get();

  private:
    // A pixel coordinate of a glyph box stays within this distance of the origin, where a float still holds every whole number.
    static constexpr float kMaxPixel = 16777216.0F;

    // The font file of a source of an ImGui font, its scale in pixels per font unit for a size of one pixel and the rasterizer of its glyphs.
    struct Source {
        text::FontFile file;
        float scale = 1.0F;
        std::unique_ptr<hb_raster_draw_t, decltype(&hb_raster_draw_destroy)> raster{nullptr, &hb_raster_draw_destroy};
    };

    // The pixels a glyph covers at a raster scale, rounded out to whole pixels, with y pointing down.
    struct Box {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
    };

    [[nodiscard]] static ImFontLoader make();

    [[nodiscard]] static bool initSource(ImFontAtlas* atlas, ImFontConfig* source);
    static void destroySource(ImFontAtlas* atlas, ImFontConfig* source);
    [[nodiscard]] static int findGlyph(ImFontAtlas* atlas, ImFontConfig* source, ImWchar codePoint);
    [[nodiscard]] static bool containsGlyph(ImFontAtlas* atlas, ImFontConfig* source, int glyph);
    [[nodiscard]] static bool initBaked(ImFontAtlas* atlas, ImFontConfig* source, ImFontBaked* baked, void* loaderData);
    [[nodiscard]] static bool loadGlyph(ImFontAtlas* atlas, ImFontConfig* source, ImFontBaked* baked, void* loaderData, int glyph, ImFontGlyph* loaded, float* advance);

    [[nodiscard]] static bool measure(const Source& source, std::uint32_t glyph, float scaleX, float scaleY, Box& box);
    static void rasterize(Source& source, std::uint32_t glyph, float scaleX, float scaleY, const Box& box, std::span<std::uint8_t> pixels, int stride);
    static void filterRows(std::span<std::uint8_t> pixels, int width, int height, int kernel);
    static void filterColumns(std::span<std::uint8_t> pixels, int width, int height, int kernel);
    [[nodiscard]] static float getOversampleShift(int oversample) noexcept;
};

} // namespace haylen::ui
