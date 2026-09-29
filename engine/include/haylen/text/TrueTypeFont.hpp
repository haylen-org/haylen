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
    // The spread is how far in pixels at the bake size the distance field reaches past the edge of a glyph, which bounds outlines, weights, glows and blurred shadows. The bake size, the spread and the atlas size must be positive and at most the maximum texture size of the device.
    struct Options {
        float bakeSize = 48.0F;
        int spread = 8;
        int atlasSize = 512;
    };

    // Reads the font file, which throws std::runtime_error when it is not a TrueType or OpenType font, and std::invalid_argument for options out of range.
    TrueTypeFont(graphics::Device& graphicsDevice, std::vector<std::uint8_t> ttf, const Options& fontOptions = kDefaultOptions);
    ~TrueTypeFont() override;

    [[nodiscard]] bool isDistanceField() const noexcept override {
        return true;
    }
    [[nodiscard]] bool hasGlyph(char32_t codePoint) override;
    void shape(const Run& run, std::vector<ShapedGlyph>& shaped) override;

    // Returns a glyph, which adds it to the atlas the first time. An index the font does not have throws std::out_of_range, and a glyph that no atlas the device allows can hold throws std::runtime_error.
    [[nodiscard]] const Glyph& getGlyph(std::uint32_t index) override;
    [[nodiscard]] std::size_t getPageCount() const noexcept override {
        return 1;
    }
    [[nodiscard]] const graphics::Texture& getPage(std::size_t index) const override;

    // Uploads the glyphs added since the last call. An atlas that grew becomes a new texture, so text queued for drawing before keeps the image its coordinates were measured on.
    void sync() override;

    // The font file itself, which the UI hands to Dear ImGui for the widgets it draws.
    [[nodiscard]] std::span<const std::uint8_t> getData() const noexcept;

  private:
    struct Face;

    static const Options kDefaultOptions;

    // A font collection header takes this many bytes, the most stb_truetype reads to find where the first font starts. The offset table of a font and each record of its table directory follow with their own sizes.
    static constexpr std::size_t kHeaderSize = 16;
    static constexpr std::size_t kOffsetTableSize = 12;
    static constexpr std::size_t kTableRecordSize = 16;

    TrueTypeFont(graphics::Device& graphicsDevice, std::unique_ptr<Face> opened, const Options& fontOptions);

    [[nodiscard]] static const Options& validate(const Options& value, const graphics::Device& graphicsDevice);
    [[nodiscard]] static bool hasTables(std::span<const std::uint8_t> ttf, std::size_t start) noexcept;
    [[nodiscard]] static std::unique_ptr<Face> open(std::vector<std::uint8_t> ttf, const Options& fontOptions);
    [[nodiscard]] static Metrics readMetrics(const Face& opened, const Options& fontOptions) noexcept;

    [[nodiscard]] Glyph rasterize(std::uint32_t index);
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
