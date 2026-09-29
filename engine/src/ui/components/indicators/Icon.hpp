#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

// A small square picture, such as an item or resource icon, tinted by a theme color when one is given.
class Icon final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "icon";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    std::string image;
    float size = 0.0F;
    std::optional<Theme::Color> color;
};

} // namespace haylen::ui
