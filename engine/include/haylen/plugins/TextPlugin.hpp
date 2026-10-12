#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/text/BitmapFont.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/RichTextOptions.hpp"
#include "haylen/text/RichTextRegistry.hpp"

namespace haylen::plugins {

// Owns the text services the engine shares: the registry of the effects and icons rich text markup names, and the `bitmapFont` asset type for BMFont files and the `gridFont` type for images of equal cells. Fonts, families and rich text reach Lua through `haylen.graphics` and `haylen.graphics2d`.
class TextPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "text";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void endFrame(core::Engine& engine) override;

    [[nodiscard]] const std::shared_ptr<text::RichTextRegistry>& getRegistry() const;

    // Returns the image of an `[img]` tag, loaded through the assets. An image stays loaded while frames keep drawing it, so text laid out again every frame loads it once, and it goes back to the assets after a frame that does not use it.
    [[nodiscard]] graphics::Texture getImage(core::Engine& engine, std::string_view path);

    // A name of a `[font]` tag and the family the options of a drawn text give it.
    using FontName = std::pair<std::string, std::shared_ptr<text::FontFamily>>;

    // Returns the rich text that immediate draws and measures of the markup with the options share, made at the first one and kept while frames keep using it, so text drawn the same way every frame lays out once. The options compare by their values, `fonts` by the names and families it lists, and the text runs at the seconds given, the time the app has run. A text that a frame does not use goes at the end of that frame.
    [[nodiscard]] text::RichText& getDrawnText(std::string_view markup, const text::RichTextOptions& options, std::span<const FontName> fonts, double seconds);

  private:
    struct DecodedFont;

    struct DrawnText {
        std::string markup;
        text::RichTextOptions options;
        std::vector<FontName> fonts;
        std::unique_ptr<text::RichText> text;
        bool used = false;
    };

    [[nodiscard]] static std::size_t hashDrawn(std::string_view markup, const text::RichTextOptions& options) noexcept;
    [[nodiscard]] static bool isSameDrawn(const DrawnText& drawn, std::string_view markup, const text::RichTextOptions& options, std::span<const FontName> fonts) noexcept;

    struct CachedImage {
        graphics::Texture texture;
        bool used = false;
    };

    [[nodiscard]] static core::Json normalizeGrid(const core::Json& options);
    [[nodiscard]] static text::BitmapFont::Grid readGrid(const core::Json& options);

    std::shared_ptr<text::RichTextRegistry> registry;
    std::map<std::string, CachedImage, std::less<>> images;
    std::unordered_multimap<std::size_t, DrawnText> drawnTexts;
};

} // namespace haylen::plugins
