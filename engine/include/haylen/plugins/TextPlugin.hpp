#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/text/BitmapFont.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/RichTextOptions.hpp"
#include "haylen/text/RichTextRegistry.hpp"
#include "haylen/text/SystemFonts.hpp"

namespace haylen::platform {
class Host;
}

namespace haylen::plugins {

// Owns the text services the engine shares: the registry of the effects and icons rich text markup names, the fonts of the system that families draw what none of their fonts holds with, and the `bitmapFont` asset type for BMFont files and the `gridFont` type for images of equal cells. Fonts, families and rich text reach Lua through `haylen.graphics` and `haylen.graphics2d`.
class TextPlugin final : public Plugin {
  public:
    explicit TextPlugin(platform::Host& platformHost) : host(platformHost) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "text";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void beginFrame(core::Engine& engine, float deltaSeconds) override;
    void endFrame(core::Engine& engine) override;

    [[nodiscard]] const std::shared_ptr<text::RichTextRegistry>& getRegistry() const;

    // The fonts of the system that the families of the engine and of the app share.
    [[nodiscard]] const std::shared_ptr<text::SystemFonts>& getSystemFonts() const noexcept {
        return systemFonts;
    }

    // Returns the image of an `[img]` tag, loaded through the assets. An image stays loaded while frames keep drawing it, so text laid out again every frame loads it once, and it goes back to the assets after a frame that does not use it.
    [[nodiscard]] graphics::Texture getImage(core::Engine& engine, std::string_view path);

    // A name of a `[font]` tag and the family the options of a drawn text give it.
    using FontName = std::pair<std::string, std::shared_ptr<text::FontFamily>>;

    // Returns the rich text that immediate draws and measures of the markup with the options share, made at the first one and kept while frames keep using it, so text drawn the same way every frame lays out once. The options compare by their values, `fonts` by the names and families it lists, and the text runs at the seconds given, the time the app has run. A text that a frame does not use goes at the end of that frame.
    [[nodiscard]] text::RichText& getDrawnText(std::string_view markup, const text::RichTextOptions& options, std::span<const FontName> fonts, double seconds);

  private:
    struct DecodedFont;

    // What a lookup of the fonts of the system found on the I/O pool: the faces it read that the engine had not loaded, with their files, the names of the faces it refused and why the faces it could not read failed.
    struct SystemLookup {
        struct Found {
            std::string name;
            int face = 0;
            std::vector<std::uint8_t> bytes;
        };

        std::vector<Found> faces;
        std::vector<std::string> refused;
        std::vector<std::string> failures;
    };

    // Looks up the code points that families asked for since the last lookup, one lookup at a time, and adds the faces it finds once they arrive.
    void lookUpSystemFonts(core::Engine& engine);
    [[nodiscard]] static SystemLookup findSystemFaces(platform::Host& target, std::span<const char32_t> codePoints, std::vector<std::string> known);
    void addSystemFonts(core::Engine& engine, core::JobSystem::Result<SystemLookup> lookup);

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

    platform::Host& host;
    std::shared_ptr<text::RichTextRegistry> registry;
    std::shared_ptr<text::SystemFonts> systemFonts;
    std::vector<char32_t> requested;
    bool lookingUp = false;

    // Lives from the start of the plugin to its stop, so a lookup that ends after the stop changes nothing.
    std::shared_ptr<bool> started;
    std::map<std::string, CachedImage, std::less<>> images;
    std::unordered_multimap<std::size_t, DrawnText> drawnTexts;
};

} // namespace haylen::plugins
