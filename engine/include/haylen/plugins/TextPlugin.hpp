#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/text/BitmapFont.hpp"
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

  private:
    struct DecodedFont;

    struct CachedImage {
        graphics::Texture texture;
        bool used = false;
    };

    [[nodiscard]] static core::Json normalizeGrid(const core::Json& options);
    [[nodiscard]] static text::BitmapFont::Grid readGrid(const core::Json& options);

    std::shared_ptr<text::RichTextRegistry> registry;
    std::map<std::string, CachedImage, std::less<>> images;
};

} // namespace haylen::plugins
