#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Alignment.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

// Rich text markup in a theme font, drawn by the 2D renderer inside the clip of the UI in the language of the node, lined up with the side of the UI its alignment names. Links are focusable items that report link when activated and linkHover when the pointer enters or leaves them, hints show as tooltips, and images load through the UI.
class RichText final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "richText";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }
    // Only text with links takes the focus. Literal markup knows its links as soon as it is set, and a translation once it has been drawn.
    [[nodiscard]] bool isFocusable() const noexcept override {
        return linked;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    static constexpr std::array<std::pair<std::string_view, Theme::Font>, 6> kFonts{{
        {"body", Theme::Font::Body},
        {"caption", Theme::Font::Caption},
        {"button", Theme::Font::Button},
        {"heading", Theme::Font::Heading},
        {"title", Theme::Font::Title},
        {"monospace", Theme::Font::Monospace},
    }};

    // Makes the rich text match the properties and the theme, keeping its effects and reveal while only the width changes.
    [[nodiscard]] text::RichText& prepare(Context& context);
    void interactWithLinks(Context& context, math::Vec2 origin);
    void showHint(Context& context, math::Vec2 origin);

    TextValue text;
    Theme::Font font = Theme::Font::Body;
    std::optional<Theme::Color> color;
    text::Alignment textAlign = text::Alignment::Start;
    bool wrap = true;
    float revealSpeed = 0.0F;
    int visibleCharacters = -1;
    bool changed = true;
    bool linked = false;
    std::shared_ptr<text::RichText> richText;
    const Context* preparedFor = nullptr;
    std::uint64_t updatedFrame = 0;
    std::optional<std::size_t> hoveredLink;
};

} // namespace haylen::ui
