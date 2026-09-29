#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include "haylen/graphics/Texture.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::graphics {
class Device;
}

namespace haylen::text {

// A TrueType or OpenType font shaped by HarfBuzz and rendered through a signed distance field atlas that grows as new glyphs are used, so text stays sharp at any size and takes outlines, weights and soft edges.
class TrueTypeFont final : public Font {
  public:
    // The spread is how far in pixels at the bake size the distance field reaches past the edge of a glyph, which bounds outlines, weights, glows and blurred shadows.
    struct Options {
        float bakeSize = 48.0F;
        int spread = 8;
        int atlasSize = 512;
    };

    TrueTypeFont(graphics::Device& graphicsDevice, std::vector<std::uint8_t> ttf, const Options& fontOptions = kDefaultOptions);
    ~TrueTypeFont() override;

    [[nodiscard]] bool isDistanceField() const noexcept override {
        return true;
    }
    [[nodiscard]] bool hasGlyph(char32_t codePoint) override;
    void shape(const Run& run, std::vector<ShapedGlyph>& shaped) override;
    [[nodiscard]] const Glyph& getGlyph(std::uint32_t index) override;
    [[nodiscard]] std::size_t getPageCount() const noexcept override {
        return 1;
    }
    [[nodiscard]] const graphics::Texture& getPage(std::size_t index) const override;
    void sync() override;

    // The font file itself, which the UI hands to Dear ImGui for the widgets it draws.
    [[nodiscard]] std::span<const std::uint8_t> getData() const noexcept;

  private:
    struct Face;

    static const Options kDefaultOptions;

    // The offset table alone takes this many bytes, and stb_truetype reads it before checking anything.
    static constexpr std::size_t kHeaderSize = 12;

    TrueTypeFont(graphics::Device& graphicsDevice, std::unique_ptr<Face> opened, const Options& fontOptions);

    [[nodiscard]] static std::unique_ptr<Face> open(std::vector<std::uint8_t> ttf, const Options& fontOptions);
    [[nodiscard]] static Metrics readMetrics(const Face& opened, const Options& fontOptions) noexcept;

    void rasterize(std::uint32_t index, Glyph& glyph);
    void grow();

    std::unique_ptr<Face> face;
    graphics::Device& device;
    Options options;
    graphics::Texture texture;
    std::unordered_map<std::uint32_t, Glyph> glyphs;
    std::unordered_map<char32_t, bool> coverage;
    std::vector<std::uint8_t> atlas;
    int atlasWidth = 0;
    int atlasHeight = 0;
    int cursorX = 1;
    int cursorY = 1;
    int rowHeight = 0;
    bool dirty = false;
};

} // namespace haylen::text
