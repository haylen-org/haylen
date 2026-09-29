#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

class Label final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "label";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
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

    TextValue text;
    Theme::Font font = Theme::Font::Body;
    std::optional<Theme::Color> color;
    Alignment textAlign = Alignment::Start;
    bool wrap = true;
    std::optional<math::Color> outline;
    float outlineWidth = 2.0F;
};

} // namespace haylen::ui
