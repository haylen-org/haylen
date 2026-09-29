#pragma once

#include <memory>

#include "haylen/core/Json.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/text/BitmapFont.hpp"
#include "haylen/text/RichTextRegistry.hpp"

namespace haylen::plugins {

// Owns the text services the engine shares: the registry of the effects and icons rich text markup names, and the bitmapFont asset type for BMFont files and the gridFont type for images of equal cells. Fonts, families and rich text reach Lua through haylen.graphics and haylen.graphics2d.
class TextPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "text";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;

    [[nodiscard]] const std::shared_ptr<text::RichTextRegistry>& getRegistry() const;

  private:
    struct DecodedFont;

    [[nodiscard]] static core::Json normalizeGrid(const core::Json& options);
    [[nodiscard]] static text::BitmapFont::Grid readGrid(const core::Json& options);

    std::shared_ptr<text::RichTextRegistry> registry;
};

} // namespace haylen::plugins
